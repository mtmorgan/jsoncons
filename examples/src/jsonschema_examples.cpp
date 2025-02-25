// Copyright 2013-2024 Daniel Parker
// Distributed under Boost license

#include <iostream>
#include <fstream>
#include <string>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpatch/jsonpatch.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

// for brevity
using jsoncons::json;
using jsoncons::ojson;
namespace jsonschema = jsoncons::jsonschema;
namespace jsonpatch = jsoncons::jsonpatch; 

void reporter_example() 
{
    // JSON Schema
    json schema = json::parse(R"(
{
  "$id": "https://example.com/arrays.schema.json",
  "$schema": "http://json-schema.org/draft-07/schema#",
  "description": "A representation of a person, company, organization, or place",
  "type": "object",
  "properties": {
    "fruits": {
      "type": "array",
      "items": {
        "type": "string"
      }
    },
    "vegetables": {
      "type": "array",
      "items": { "$ref": "#/definitions/veggie" }
    }
  },
  "definitions": {
    "veggie": {
      "type": "object",
      "required": [ "veggieName", "veggieLike" ],
      "properties": {
        "veggieName": {
          "type": "string",
          "description": "The name of the vegetable."
        },
        "veggieLike": {
          "type": "boolean",
          "description": "Do I like this vegetable?"
        }
      }
    }
  }
}
    )");

    // Data
    json data = json::parse(R"(
{
  "fruits": [ "apple", "orange", "pear" ],
  "vegetables": [
    {
      "veggieName": "potato",
      "veggieLike": true
    },
    {
      "veggieName": "broccoli",
      "veggieLike": "false"
    },
    {
      "veggieName": "carrot",
      "veggieLike": false
    },
    {
      "veggieName": "Swiss Chard"
    }
  ]
}
   )");

    try
    {
        // Throws schema_error if JSON Schema loading fails
        jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema);

        std::size_t error_count = 0;
        auto reporter = [&error_count](const jsonschema::validation_message& message)
        {
            ++error_count;
            std::cout << message.instance_location().string() << ": " << message.message() << "\n";
        };

        // Will call reporter for each schema violation
        compiled.validate(data, reporter);

        std::cout << "\nError count: " << error_count << "\n\n";
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << "\n";
    }
}

// Until 0.174.0, throw instead of returning json::null() 
json resolver(const jsoncons::uri& uri)
{
    std::cout << "uri: " << uri.string() << ", path: " << uri.path() << "\n\n";

    std::string pathname = "./input/jsonschema/";
    pathname += std::string(uri.path());

    std::fstream is(pathname.c_str());

    return !is ? json::null() : json::parse(is);        
}

void uriresolver_example()
{ 
    // JSON Schema
    json schema = json::parse(R"(
{
    "$id": "http://localhost:1234/object",
    "type": "object",
    "properties": {
        "name": {"$ref": "name.json#/definitions/orNull"}
    }
}
    )");

    // Data
    json data = json::parse(R"(
{
    "name": {
        "name": null
    }
}
    )");

    try
    {
        // Throws schema_error if JSON Schema loading fails
        jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema, resolver);

        std::size_t error_count = 0;
        auto reporter = [&error_count](const jsonschema::validation_message& message)
        {
            ++error_count;
            std::cout << message.instance_location().string() << ": " << message.message() << "\n";
        };

        // Will call reporter for each schema violation
        compiled.validate(data, reporter);

        std::cout << "\nError count: " << error_count << "\n\n";
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << '\n';
    }
}

void defaults_example() 
{
    // JSON Schema
    json schema = json::parse(R"(
{
    "properties": {
    "bar": {
        "type": "string",
        "minLength": 4,
        "default": "bad"
    }
    }
}
)");

    try
    {
        // Data
        json data = json::parse("{}");

        // will throw schema_error if JSON Schema loading fails 
        jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema, resolver); 

        // will throw a validation_error when a schema violation happens 
        json patch;
        compiled.validate(data, patch); 

        std::cout << "Patch: " << patch << "\n";

        std::cout << "Original data: " << data << "\n";

        jsonpatch::apply_patch(data, patch);

        std::cout << "Patched data: " << data << "\n\n";
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << "\n";
    }
}

#if defined(JSONCONS_HAS_STD_VARIANT)

#include <variant>

namespace ns {

    struct os_properties {
        std::string command;
    };

    struct db_properties {
        std::string query;
    };

    struct api_properties {
        std::string target;
    };

    struct job_properties {
        std::string name;
        std::variant<os_properties,db_properties,api_properties> run;
    };

} // namespace ns

    JSONCONS_N_MEMBER_TRAITS(ns::os_properties, 1, command)
    JSONCONS_N_MEMBER_TRAITS(ns::db_properties, 1, query)
    JSONCONS_N_MEMBER_TRAITS(ns::api_properties, 1, target)
    JSONCONS_N_MEMBER_TRAITS(ns::job_properties, 2, name, run)

std::string test_schema = R"(
{
  "title": "job",
  "description": "job properties json schema",
  "definitions": {
    "os_properties": {
      "type": "object",
      "properties": {
        "command": {
          "description": "this is the OS command to run",
          "type": "string",
          "minLength": 1
        }
      },
      "required": [ "command" ],
      "additionalProperties": false
    },
    "db_properties": {
      "type": "object",
      "properties": {
        "query": {
          "description": "this is db query to run",
          "type": "string",
          "minLength": 1
        }
      },
      "required": [ "query" ],
      "additionalProperties": false
    },

    "api_properties": {
      "type": "object",
      "properties": {
        "target": {
          "description": "this is api target to run",
          "type": "string",
          "minLength": 1
        }
      },
      "required": [ "target" ],
      "additionalProperties": false
    }
  },

  "type": "object",
  "properties": {
    "name": {
      "description": "name of the flow",
      "type": "string",
      "minLength": 1
    },
    "run": {
      "description": "job run properties",
      "type": "object",
      "oneOf": [

        { "$ref": "#/definitions/os_properties" },
        { "$ref": "#/definitions/db_properties" },
        { "$ref": "#/definitions/api_properties" }

      ]
    }
  },
  "required": [ "name", "run" ],
  "additionalProperties":  false
}
)";

std::string test_data = R"(
{
    "name": "testing flow", 
    "run" : {
            "command": "some command"    
            }
}

)";

void validate_before_decode_example() 
{
    try
    {
        json schema = json::parse(test_schema);
        json data = json::parse(test_data);

        // Throws schema_error if JSON Schema loading fails
        jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema);

        // Test that input is valid before attempting to decode
        if (compiled.is_valid(data))
        {
            const ns::job_properties v = data.as<ns::job_properties>(); // You don't need to reparse test_data 

            std::string output;
            jsoncons::encode_json_pretty(v, output);
            std::cout << output << std::endl;

            // Verify that output is valid
            json test = json::parse(output);
            assert(compiled.is_valid(test));
        }
        else
        {
            std::cout << "Invalid input\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << '\n';
    }
}

#endif // JSONCONS_HAS_STD_VARIANT

void draft_201212_example()
{
    json schema = json::parse(R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": "https://test.json-schema.org/typical-dynamic-resolution/root",
    "$ref": "list",
    "$defs": {
        "foo": {
            "$dynamicAnchor": "items",
            "type": "string"
        },
        "list": {
            "$id": "list",
            "type": "array",
            "items": { "$dynamicRef": "#items" },
            "$defs": {
              "items": {
                  "$comment": "This is only needed to satisfy the bookending requirement",
                  "$dynamicAnchor": "items"
              }
            }
        }
    }
}
)");

    jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema);

    json data = json::parse(R"(["foo", 42])");

    jsoncons::json_decoder<ojson> decoder;
    compiled.validate(data, decoder);
    ojson output = decoder.get_result();
    std::cout << pretty_print(output) << "\n\n";
}

void draft_201909_example()
{
    json schema = json::parse(R"(
{
    "$schema": "https://json-schema.org/draft/2019-09/schema",
    "type": "object",
    "properties": {
        "foo": { "type": "string" }
    },
    "allOf": [
        {
            "properties": {
                "bar": { "type": "string" }
            }
        }
    ],
    "unevaluatedProperties": false
}
)");

    jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema);

    json data = json::parse(R"({"foo": "foo","bar": "bar","baz": "baz"})");

    jsoncons::json_decoder<ojson> decoder;
    compiled.validate(data, decoder);
    ojson output = decoder.get_result();
    std::cout << pretty_print(output) << "\n\n";
}

void draft_07_example()
{
    json schema = json::parse(R"(
{
    "items": [{}],
    "additionalItems": {"type": "integer"}
}
)");

    // Need to supply default version because schema does not have $schema keyword  
    jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema,
        jsonschema::evaluation_options{}.default_version(jsonschema::schema_version::draft7()));

    json data = json::parse(R"([ null, 2, 3, "foo" ])");

    jsoncons::json_decoder<ojson> decoder;
    compiled.validate(data, decoder);
    ojson output = decoder.get_result();
    std::cout << pretty_print(output) << "\n\n";
}

void cross_schema_example()
{
    json schema = json::parse(R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": "https://example.com/schema",
    "$defs": {
        "foo": {
            "$schema": "http://json-schema.org/draft-07/schema#",
            "$id": "schema/foo",
            "definitions" : {
                "bar" : {
                    "type" : "string"
                }               
            }
        }       
    },
    "properties" : {
        "thing" : {
            "$ref" : "schema/foo#/definitions/bar"
        }
    }
}
)");
    jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(schema);

    json data = json::parse(R"({"thing" : 10})");

    jsoncons::json_decoder<ojson> decoder;
    compiled.validate(data, decoder);
    ojson output = decoder.get_result();
    std::cout << pretty_print(output) << "\n\n";
}

int main()
{
    std::cout << "\nJSON Schema Examples\n\n";

    reporter_example();
    uriresolver_example();
    defaults_example();

#if defined(JSONCONS_HAS_STD_VARIANT)
    validate_before_decode_example();
#endif

    draft_201212_example();
    draft_201909_example();
    draft_07_example();
    
    cross_schema_example();
    
    std::cout << "\n";
}

