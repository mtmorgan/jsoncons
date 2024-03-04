// Copyright 2013-2023 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_JSONSCHEMA_COMMON_SCHEMA_BUILDER_HPP
#define JSONCONS_JSONSCHEMA_COMMON_SCHEMA_BUILDER_HPP

#include <memory>
#include <jsoncons_ext/jsonschema/common/keyword_validators.hpp>
#include <jsoncons_ext/jsonschema/common/compilation_context.hpp>

namespace jsoncons {
namespace jsonschema {

    template <class Json>
    class schema_builder
    {
    public:
        using keyword_validator_type = typename std::unique_ptr<keyword_validator<Json>>;
        using schema_validator_pointer = schema_validator<Json>*;
        using schema_validator_type = typename std::unique_ptr<schema_validator<Json>>;
        using ref_validator_type = ref_validator<Json>;
        using ref_type = ref<Json>;

    private:
        uri_resolver<Json> resolver_;
        schema_validator_type root_;

        // Owns all subschemas
        std::vector<schema_validator_type> subschemas_;
    public:
        std::map<jsoncons::uri, schema_validator_pointer> schema_dictionary_; 
        std::vector<std::pair<jsoncons::uri, ref_type*>> unresolved_refs_; 
        std::map<jsoncons::uri, Json> unknown_keywords_;

    public:

        schema_builder() = default;

        schema_builder(uri_resolver<Json> resolver)
            : resolver_(resolver)
        {
        }

        virtual ~schema_builder() = default;

        void save_schema(schema_validator_type&& schema)
        {
            subschemas_.emplace_back(std::move(schema));
        }

        void build_schema(const Json& sch, const std::string& retrieval_uri) 
        {
            root_ = make_schema_validator(compilation_context(uri_wrapper(retrieval_uri)), sch, {});
        }

        std::shared_ptr<json_schema<Json>> get_schema()
        {                        
            std::cout << "schema_dictionary:\n";
            for (auto& member : schema_dictionary_)
            {
                std::cout << "    " << member.first.string() << "\n";
            }

            // load all external schemas that have not already been loaded
            std::size_t loaded_count = 0;
            do
            {
                loaded_count = 0;

                std::vector<jsoncons::uri> locations;
                for (const auto& item : unresolved_refs_)
                {
                    locations.push_back(item.first);
                }
                std::cout << "unresolved_refs:\n";
                for (auto& member : unresolved_refs_)
                {
                    std::cout << "    " << member.first.string() << "\n";
                }

                for (const auto& loc : locations)
                {
                    if (schema_dictionary_.find(loc) == schema_dictionary_.end()) // registry for this file is empty
                    {
                        if (resolver_)
                        {
                            Json external_sch = resolver_(loc.base());
                            this->save_schema(make_schema_validator(compilation_context(uri_wrapper(loc.base())), external_sch, {}));
                            ++loaded_count;
                        }
                        else
                        {
                            JSONCONS_THROW(schema_error("External schema reference '" + loc.base().string() + "' needs to be loaded, but no resolver provided"));
                        }
                    }
                }
            } while (loaded_count > 0);

            resolve_references();

            return std::make_shared<json_schema<Json>>(std::move(subschemas_), std::move(root_));
        }

        void resolve_references()
        {
            for (auto& ref : unresolved_refs_)
            {
                auto it = schema_dictionary_.find(ref.first);
                if (it == schema_dictionary_.end())
                {
                    JSONCONS_THROW(schema_error("Undefined reference " + ref.first.string()));
                }
                ref.second->set_referred_schema(it->second);
            }
        }

        virtual const char* schema_version() const noexcept = 0;

        virtual compilation_context make_compilation_context(const compilation_context& parent,
            const Json& sch, jsoncons::span<const std::string> keys) const = 0;

        virtual schema_validator_type make_schema_validator(const compilation_context& context, 
            const Json& sch, jsoncons::span<const std::string> keys) = 0;

        virtual std::unique_ptr<max_length_validator<Json>> make_max_length_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("maxLength");
            if (!sch.is_number())
            {
                std::string message("maxLength must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::size_t>();
            return jsoncons::make_unique<max_length_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<min_length_validator<Json>> make_min_length_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("minLength");
            if (!sch.is_number())
            {
                std::string message("minLength must be an integer value");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::size_t>();
            return jsoncons::make_unique<min_length_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<not_validator<Json>> make_not_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("not");
            std::string not_key[] = { "not" };
            return jsoncons::make_unique<not_validator<Json>>( schema_path, 
                make_schema_validator(context, sch, not_key));
        }

        virtual std::unique_ptr<const_validator<Json>> make_const_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("const");
            return jsoncons::make_unique<const_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<enum_validator<Json>> make_enum_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("enum");
            return jsoncons::make_unique<enum_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<required_validator<Json>> make_required_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("required");
            return jsoncons::make_unique<required_validator<Json>>( schema_path, sch.template as<std::vector<std::string>>());
        }

        virtual std::unique_ptr<maximum_validator<Json>> make_maximum_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("maximum");
            if (!sch.is_number())
            {
                std::string message("maximum must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            return jsoncons::make_unique<maximum_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<exclusive_maximum_validator<Json>> make_exclusive_maximum_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("exclusiveMaximum");
            if (!sch.is_number())
            {
                std::string message("exclusiveMaximum must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            return jsoncons::make_unique<exclusive_maximum_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<keyword_validator<Json>> make_minimum_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("minimum");
                
            if (!sch.is_number())
            {
                std::string message("minimum must be an integer");
                JSONCONS_THROW(schema_error(message));
            }
            return jsoncons::make_unique<minimum_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<exclusive_minimum_validator<Json>> make_exclusive_minimum_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("exclusiveMinimum");
            if (!sch.is_number())
            {
                std::string message("exclusiveMinimum must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            return jsoncons::make_unique<exclusive_minimum_validator<Json>>( schema_path, sch);
        }

        virtual std::unique_ptr<multiple_of_validator<Json>> make_multiple_of_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("multipleOf");
            if (!sch.is_number())
            {
                std::string message("multipleOf must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<double>();
            return jsoncons::make_unique<multiple_of_validator<Json>>( schema_path, value);
        }


        virtual std::unique_ptr<type_validator<Json>> make_type_validator(const compilation_context& context,
            const Json& sch)
        {
            std::string schema_path = context.get_absolute_uri().string();
            std::vector<json_schema_type> expected_types;

            switch (sch.type()) 
            { 
                case json_type::string_value: 
                {
                    auto type = sch.template as<std::string>();
                    if (type == "null")
                    {
                        expected_types.push_back(json_schema_type::null);
                    }
                    else if (type == "object")
                    {
                        expected_types.push_back(json_schema_type::object);
                    }
                    else if (type == "array")
                    {
                        expected_types.push_back(json_schema_type::array);
                    }
                    else if (type == "string")
                    {
                        expected_types.push_back(json_schema_type::string);
                    }
                    else if (type == "boolean")
                    {
                        expected_types.push_back(json_schema_type::boolean);
                    }
                    else if (type == "integer")
                    {
                        expected_types.push_back(json_schema_type::integer);
                    }
                    else if (type == "number")
                    {
                        expected_types.push_back(json_schema_type::number);
                    }
                    break;
                } 

                case json_type::array_value: // "type": ["type1", "type2"]
                {
                    for (const auto& item : sch.array_range())
                    {
                        auto type = item.template as<std::string>();
                        if (type == "null")
                        {
                            expected_types.push_back(json_schema_type::null);
                        }
                        else if (type == "object")
                        {
                            expected_types.push_back(json_schema_type::object);
                        }
                        else if (type == "array")
                        {
                            expected_types.push_back(json_schema_type::array);
                        }
                        else if (type == "string")
                        {
                            expected_types.push_back(json_schema_type::string);
                        }
                        else if (type == "boolean")
                        {
                            expected_types.push_back(json_schema_type::boolean);
                        }
                        else if (type == "integer")
                        {
                            expected_types.push_back(json_schema_type::integer);
                        }
                        else if (type == "number")
                        {
                            expected_types.push_back(json_schema_type::number);
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            return jsoncons::make_unique<type_validator<Json>>( std::move(schema_path), 
                std::move(expected_types)
         );
        }

        virtual std::unique_ptr<content_encoding_validator<Json>> make_content_encoding_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("contentEncoding");
            if (!sch.is_string())
            {
                std::string message("contentEncoding must be a string");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::string>();
            return jsoncons::make_unique<content_encoding_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<content_media_type_validator<Json>> make_content_media_type_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("contentMediaType");
            if (!sch.is_string())
            {
                std::string message("contentMediaType must be a string");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::string>();
            return jsoncons::make_unique<content_media_type_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<format_validator<Json>> make_format_validator(const compilation_context& context, const Json& sch)
        {
            auto schema_path = context.make_schema_path_with("format");

            std::string format = sch.template as<std::string>();

            format_checker format_check;
            if (format == "date-time")
            {
                format_check = rfc3339_date_time_check;
            }
            else if (format == "date") 
            {
                format_check = rfc3339_date_check;
            } 
            else if (format == "time") 
            {
                format_check = rfc3339_time_check;
            } 
            else if (format == "email") 
            {
                format_check = email_check;
            } 
            else if (format == "hostname") 
            {
                format_check = hostname_check;
            } 
            else if (format == "ipv4") 
            {
                format_check = ipv4_check;
            } 
            else if (format == "ipv6") 
            {
                format_check = ipv6_check;
            } 
            else if (format == "regex") 
            {
                format_check = regex_check;
            } 
            else
            {
                // Not supported - ignore
                format_check = nullptr;
            }       

            return jsoncons::make_unique<format_validator<Json>>( schema_path, 
                format_check);
        }

        virtual std::unique_ptr<pattern_validator<Json>> make_pattern_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("pattern");
            auto pattern_string = sch.template as<std::string>();
            auto regex = std::regex(pattern_string, std::regex::ECMAScript);
            return jsoncons::make_unique<pattern_validator<Json>>( schema_path, 
                pattern_string, regex);
        }

        virtual std::unique_ptr<max_items_validator<Json>> make_max_items_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("maxItems");
            if (!sch.is_number())
            {
                std::string message("maxItems must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::size_t>();
            return jsoncons::make_unique<max_items_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<min_items_validator<Json>> make_min_items_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("minItems");
            if (!sch.is_number())
            {
                std::string message("minItems must be a number value");
                JSONCONS_THROW(schema_error(message));
            }
            auto value = sch.template as<std::size_t>();
            return jsoncons::make_unique<min_items_validator<Json>>( schema_path, value);
        }

        virtual std::unique_ptr<contains_validator<Json>> make_contains_validator(const compilation_context& context,
            const Json& sch, const Json& parent)
        {
            uri schema_path = context.make_schema_path_with("contains");

            std::string sub_keys[] = { "contains" };

            std::unique_ptr<max_contains_keyword<Json>> max_contains;
            auto it = parent.find("maxContains");
            if (it != parent.object_range().end()) 
            {
                uri path = context.make_schema_path_with("maxContains");
                auto value = it->value().template as<std::size_t>();
                max_contains = jsoncons::make_unique<max_contains_keyword<Json>>(path, value);
            }
            else
            {
                uri path = context.make_schema_path_with("maxContains");
                max_contains = jsoncons::make_unique<max_contains_keyword<Json>>(path, (std::numeric_limits<std::size_t>::max)());
            }

            std::unique_ptr<min_contains_keyword<Json>> min_contains;
            it = parent.find("minContains");
            if (it != parent.object_range().end()) 
            {
                uri path = context.make_schema_path_with("minContains");
                auto value = it->value().template as<std::size_t>();
                min_contains = jsoncons::make_unique<min_contains_keyword<Json>>(path, value);
            }
            else
            {
                uri path = context.make_schema_path_with("minContains");
                min_contains = jsoncons::make_unique<min_contains_keyword<Json>>(path, 1);
            }

            return jsoncons::make_unique<contains_validator<Json>>( schema_path, 
                make_schema_validator(context, sch, sub_keys), std::move(max_contains), std::move(min_contains));
        }

        virtual std::unique_ptr<unique_items_validator<Json>> make_unique_items_validator(const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("uniqueItems");
            bool are_unique = sch.template as<bool>();
            return jsoncons::make_unique<unique_items_validator<Json>>( schema_path, are_unique);
        }

        virtual std::unique_ptr<combining_validator<Json,all_of_criterion<Json>>> make_all_of_validator(const compilation_context& context,
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("allOf");
            std::vector<schema_validator_type> subschemas;

            size_t c = 0;
            for (const auto& subsch : sch.array_range())
            {
                std::string sub_keys[] = { all_of_criterion<Json>::key(), std::to_string(c++) };
                subschemas.emplace_back(make_schema_validator(context, subsch, sub_keys));
            }
            return jsoncons::make_unique<combining_validator<Json,all_of_criterion<Json>>>(std::move(schema_path), std::move(subschemas));
        }

        virtual std::unique_ptr<combining_validator<Json,any_of_criterion<Json>>> make_any_of_validator(const compilation_context& context,
            const Json& sch)
        {
            uri schema_path = context.make_schema_path_with("anyOf");
            std::vector<schema_validator_type> subschemas;

            size_t c = 0;
            for (const auto& subsch : sch.array_range())
            {
                std::string sub_keys[] = { any_of_criterion<Json>::key(), std::to_string(c++) };
                subschemas.emplace_back(make_schema_validator(context, subsch, sub_keys));
            }
            return jsoncons::make_unique<combining_validator<Json,any_of_criterion<Json>>>(std::move(schema_path), std::move(subschemas));
        }

        virtual std::unique_ptr<combining_validator<Json,one_of_criterion<Json>>> make_one_of_validator(const compilation_context& context,
            const Json& sch)
        {
            uri schema_path{ context.make_schema_path_with("oneOf") };
            std::vector<schema_validator_type> subschemas;

            size_t c = 0;
            for (const auto& subsch : sch.array_range())
            {
                std::string sub_keys[] = { one_of_criterion<Json>::key(), std::to_string(c++) };
                subschemas.emplace_back(make_schema_validator(context, subsch, sub_keys));
            }
            return jsoncons::make_unique<combining_validator<Json,one_of_criterion<Json>>>(std::move(schema_path), std::move(subschemas));
        }

        virtual std::unique_ptr<dependencies_validator<Json>> make_dependencies_validator(const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.get_absolute_uri();
            std::map<std::string, keyword_validator_type> dependent_required;
            std::map<std::string, schema_validator_type> dependent_schemas;

            for (const auto& dep : sch.object_range())
            {
                switch (dep.value().type()) 
                {
                    case json_type::array_value:
                    {
                        auto location = context.make_schema_path_with("dependencies");
                        dependent_required.emplace(dep.key(), 
                            this->make_required_validator(compilation_context(std::vector<uri_wrapper>{{uri_wrapper{ location }}}),
                                dep.value().template as<std::vector<std::string>>()));
                        break;
                    }
                    case json_type::object_value:
                    {
                        std::string sub_keys[] = {"dependencies"};
                        dependent_schemas.emplace(dep.key(),
                            make_schema_validator(context, dep.value(), sub_keys));
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }         

            return jsoncons::make_unique<dependencies_validator<Json>>( std::move(schema_path),
                std::move(dependent_required), std::move(dependent_schemas));
        }

        virtual std::unique_ptr<property_names_validator<Json>> make_property_names_validator(
            const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.get_absolute_uri();
            schema_validator_type property_names_schema_validator;

            std::string sub_keys[] = { "propertyNames"};
            property_names_schema_validator = make_schema_validator(context, sch, sub_keys);

            return jsoncons::make_unique<property_names_validator<Json>>( std::move(schema_path),
                std::move(property_names_schema_validator));
        }

        // 201909 and later
                
        virtual std::unique_ptr<dependent_required_validator<Json>> make_dependent_required_validator( 
            const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.get_absolute_uri();
            std::map<std::string, keyword_validator_type> dependent_required;

            for (const auto& dep : sch.object_range())
            {
                switch (dep.value().type()) 
                {
                    case json_type::array_value:
                    {
                        auto location = context.make_schema_path_with("dependentRequired");
                        dependent_required.emplace(dep.key(), 
                            this->make_required_validator(compilation_context(std::vector<uri_wrapper>{{uri_wrapper{ location }}}),
                                dep.value()));
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }

            return jsoncons::make_unique<dependent_required_validator<Json>>( std::move(schema_path),
                std::move(dependent_required));
        }

        virtual std::unique_ptr<dependent_schemas_validator<Json>> make_dependent_schemas_validator( const compilation_context& context, 
            const Json& sch)
        {
            uri schema_path = context.get_absolute_uri();
            std::map<std::string, schema_validator_type> dependent_schemas;

            for (const auto& dep : sch.object_range())
            {
                switch (dep.value().type()) 
                {
                    case json_type::object_value:
                    {
                        std::string sub_keys[] = {"dependentSchemas"};
                        dependent_schemas.emplace(dep.key(),
                            make_schema_validator(context, dep.value(), sub_keys));
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            

            return jsoncons::make_unique<dependent_schemas_validator<Json>>( std::move(schema_path),
                std::move(dependent_schemas));
        }

        virtual std::unique_ptr<unevaluated_properties_validator<Json>> make_unevaluated_properties_validator(
            const compilation_context& context, const Json& sch)
        {
            uri schema_path = context.get_absolute_uri();

            std::string sub_keys[] = {"unevaluatedProperties"};

            return jsoncons::make_unique<unevaluated_properties_validator<Json>>( std::move(schema_path),
                make_schema_validator(context, sch, sub_keys));
        }

        void insert_schema(const uri_wrapper& identifier, schema_validator<Json>* s)
        {
            this->schema_dictionary_.emplace(identifier.uri(), s);
        }

        void insert_unknown_keyword(const uri_wrapper& uri, 
                                    const std::string& key, 
                                    const Json& value)
        {
            auto new_u = uri.append(key);
            uri_wrapper new_uri(new_u);

            if (new_uri.has_fragment() && !new_uri.has_plain_name_fragment()) 
            {
                // is there a reference looking for this unknown-keyword, which is thus no longer a unknown keyword but a schema
                auto unresolved_refs = std::find_if(this->unresolved_refs_.begin(), this->unresolved_refs_.end(),
                    [new_uri](const std::pair<jsoncons::uri,ref<Json>*>& pr) {return pr.first == new_uri.uri();});
                if (unresolved_refs != this->unresolved_refs_.end())
                {
                    this->save_schema(make_schema_validator(compilation_context(new_uri), value, {}));
                }
                else // no, nothing ref'd it, keep for later
                {
                    //file.unknown_keywords.emplace(fragment, value);
                    this->unknown_keywords_.emplace(new_uri.uri(), value);
                }

                // recursively add possible subschemas of unknown keywords
                if (value.type() == json_type::object_value)
                {
                    for (const auto& subsch : value.object_range())
                    {
                        insert_unknown_keyword(new_uri, subsch.key(), subsch.value());
                    }
                }
            }
        }

        keyword_validator_type get_or_create_reference(const uri_wrapper& identifier)
        {
            // a schema already exists
            auto it = this->schema_dictionary_.find(identifier.uri());
            if (it != this->schema_dictionary_.end())
            {
                return jsoncons::make_unique<ref_validator_type>(identifier.base(), it->second);
            }

            // referencing an unknown keyword, turn it into schema
            //
            // an unknown keyword can only be referenced by a JSONPointer,
            // not by a plain name identifier
            if (identifier.has_fragment() && !identifier.has_plain_name_fragment()) 
            {
                //std::string fragment = std::string(identifier.fragment());

                auto it2 = this->unknown_keywords_.find(identifier.uri());
                if (it2 != this->unknown_keywords_.end())
                {
                    auto& subsch = it2->second;
                    auto s = make_schema_validator(compilation_context(identifier), subsch, {});
                    this->unknown_keywords_.erase(it2);
                    auto orig = jsoncons::make_unique<ref_validator_type>(identifier.base(), s.get());
                    this->save_schema(std::move(s));
                    return orig;
                }
            }

            // get or create a ref_validator
            auto orig = jsoncons::make_unique<ref_validator_type>(identifier.base());

            this->unresolved_refs_.emplace_back(identifier.uri(), orig.get());
            return orig;
        }

    };

} // namespace jsonschema
} // namespace jsoncons

#endif // JSONCONS_JSONSCHEMA_COMMON_SCHEMA_HPP
