// Copyright 2013-2024 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JSONSCHEMA_JSON_SCHEMA_FACTORY_HPP
#define JSONCONS_JSONSCHEMA_JSON_SCHEMA_FACTORY_HPP

#include <jsoncons_ext/jsonschema/draft4/schema_builder_4.hpp>
#include <jsoncons_ext/jsonschema/draft6/schema_builder_6.hpp>
#include <jsoncons_ext/jsonschema/draft7/schema_builder_7.hpp>
#include <jsoncons_ext/jsonschema/draft201909/schema_builder_201909.hpp>
#include <jsoncons_ext/jsonschema/draft202012/schema_builder_202012.hpp>

namespace jsoncons {
namespace jsonschema {

    template <class Json>
    class schema_builder_factory
    {
    public:
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;

        schema_builder_factory()
        {
        }
            
        std::unique_ptr<schema_builder<Json>> operator()(const Json& sch, 
            const evaluation_options& options, schema_store_type* schema_store_ptr,
            const std::vector<schema_resolver<json>>& resolvers) const
        {
            std::unique_ptr<schema_builder<Json>> builder;

            if (sch.is_object())
            {
                auto it = sch.find("$schema");
                if (it != sch.object_range().end())
                { 
                    builder = get_builder(it->value().as_string_view(), options, schema_store_ptr, resolvers);
                    if (!builder)
                    {
                        std::string message("Unsupported schema version ");
                        message.append(it->value().template as<std::string>());
                        JSONCONS_THROW(schema_error(message));
                    }
                }
                else 
                {
                    if (options.default_version() == schema_version::draft202012())
                    {
                        builder = jsoncons::make_unique<jsoncons::jsonschema::draft202012::schema_builder_202012<Json>>(*this, 
                            options, schema_store_ptr, resolvers);
                    }
                    else if (options.default_version() == schema_version::draft201909())
                    {
                        builder = jsoncons::make_unique<jsoncons::jsonschema::draft201909::schema_builder_201909<Json>>(*this, 
                            options, schema_store_ptr, resolvers);
                    }
                    else if (options.default_version() == schema_version::draft7())
                    {
                        builder = jsoncons::make_unique<jsoncons::jsonschema::draft7::schema_builder_7<Json>>(*this, 
                            options, schema_store_ptr, resolvers);
                    }
                    else if (options.default_version() == schema_version::draft6())
                    {
                        builder = jsoncons::make_unique<jsoncons::jsonschema::draft6::schema_builder_6<Json>>(*this, 
                            options, schema_store_ptr, resolvers);
                    }
                    else if (options.default_version() == schema_version::draft4())
                    {
                        builder = jsoncons::make_unique<jsoncons::jsonschema::draft4::schema_builder_4<Json>>(*this, 
                            options, schema_store_ptr, resolvers);
                    }
                    else
                    {
                        JSONCONS_THROW(schema_error("Unsupported schema version " + options.default_version()));
                    }
                }
            }
            else if (sch.is_bool())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft7::schema_builder_7<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            else
            {
                 JSONCONS_THROW(schema_error("Schema must be object or boolean"));
            }
            return builder;
        }

        std::unique_ptr<schema_builder<Json>> get_builder(const jsoncons::string_view& schema_id,
            const evaluation_options& options, schema_store_type* schema_store_ptr,
            const std::vector<schema_resolver<json>>& resolvers) const
        {
            std::unique_ptr<schema_builder<Json>> builder;

            if (schema_id == schema_version::draft202012())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft202012::schema_builder_202012<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            else if (schema_id == schema_version::draft201909())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft201909::schema_builder_201909<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            else if (schema_id == schema_version::draft7())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft7::schema_builder_7<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            else if (schema_id == schema_version::draft6())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft6::schema_builder_6<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            else if (schema_id == schema_version::draft4())
            {
                builder = jsoncons::make_unique<jsoncons::jsonschema::draft4::schema_builder_4<Json>>(*this, 
                    options, schema_store_ptr, resolvers);
            }
            return builder;
        }
    };

    template <class Json>
    Json meta_resolver(const jsoncons::uri& uri)
    {
        if (uri.base() == schema_version::draft202012())
        {
            return jsoncons::jsonschema::draft202012::schema_draft202012<Json>::get_schema();
        }
        else if (uri.base() == schema_version::draft201909())
        {
            return jsoncons::jsonschema::draft201909::schema_draft201909<Json>::get_schema();
        }
        else if (uri.base() == schema_version::draft7())
        {
            return jsoncons::jsonschema::draft7::schema_draft7<Json>::get_schema();
        }
        else if (uri.base() == schema_version::draft6())
        {
            return jsoncons::jsonschema::draft6::schema_draft6<Json>::get_schema();
        }
        else if (uri.base() == schema_version::draft4())
        {
            return jsoncons::jsonschema::draft4::schema_draft4<Json>::get_schema();
        }
        else
        {
            return Json::null();
        }
    }

    template <class Json,class URIResolver>
    typename std::enable_if<extension_traits::is_unary_function_object_exact<URIResolver,Json,std::string>::value,json_schema<Json>>::type
    make_json_schema(const Json& sch, const std::string& retrieval_uri, const URIResolver& resolver, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};
        
        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>, resolver}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch, retrieval_uri);
        return json_schema<Json>(schema_builder->get_schema());
    }

    template <class Json>
    json_schema<Json> make_json_schema(const Json& sch, const std::string& retrieval_uri, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch, retrieval_uri);
        return json_schema<Json>(schema_builder->get_schema());
    }

    template <class Json,class URIResolver>
    typename std::enable_if<extension_traits::is_unary_function_object_exact<URIResolver,Json,std::string>::value,json_schema<Json>>::type
    make_json_schema(const Json& sch, const URIResolver& resolver, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>, resolver}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch);
        return json_schema<Json>(schema_builder->get_schema());
    }

    template <class Json>
    json_schema<Json> make_json_schema(const Json& sch, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch);
        return json_schema<Json>(schema_builder->get_schema());
    }
    
    // Legacy
    template <class Json,class URIResolver>
    typename std::enable_if<extension_traits::is_unary_function_object_exact<URIResolver,Json,std::string>::value,std::shared_ptr<json_schema<Json>>>::type
    make_schema(const Json& sch, const std::string& retrieval_uri, const URIResolver& resolver, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>, resolver}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch, retrieval_uri);
        return std::make_shared<json_schema<Json>>(schema_builder->get_schema());
    }

    template <class Json>
    std::shared_ptr<json_schema<Json>> make_schema(const Json& sch, const std::string& retrieval_uri, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch, retrieval_uri);
        return std::make_shared<json_schema<Json>>(schema_builder->get_schema());
    }

    template <class Json,class URIResolver>
    typename std::enable_if<extension_traits::is_unary_function_object_exact<URIResolver,Json,std::string>::value,std::shared_ptr<json_schema<Json>>>::type
    make_schema(const Json& sch, const URIResolver& resolver, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>, resolver}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch);
        return std::make_shared<json_schema<Json>>(schema_builder->get_schema());
    }

    template <class Json>
    std::shared_ptr<json_schema<Json>> make_schema(const Json& sch, 
        evaluation_options options = evaluation_options{})
    {
        using schema_store_type = std::map<jsoncons::uri, schema_validator<Json>*>;
        schema_store_type schema_store; 
        schema_builder_factory<Json> builder_factory{};

        std::vector<schema_resolver<Json>> resolvers = {{meta_resolver<Json>}};
        auto schema_builder = builder_factory(sch, options, &schema_store, resolvers);

        schema_builder->build_schema(sch);
        return std::make_shared<json_schema<Json>>(schema_builder->get_schema());
    }

} // namespace jsonschema
} // namespace jsoncons

#endif // JSONCONS_JSONSCHEMA_SCHEMA_HPP
