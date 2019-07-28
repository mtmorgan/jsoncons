// Copyright 2015 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CSV_CSV_PARSER_HPP
#define JSONCONS_CSV_CSV_PARSER_HPP

#include <memory> // std::allocator
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <system_error>
#include <cctype>
#include <jsoncons/json_exception.hpp>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/json_reader.hpp>
#include <jsoncons/json_filter.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons/detail/parse_number.hpp>
#include <jsoncons_ext/csv/csv_error.hpp>
#include <jsoncons_ext/csv/csv_options.hpp>

namespace jsoncons { namespace csv {

enum class csv_mode 
{
    initial,
    header,
    data,
    subfields
};

enum class csv_parse_state 
{
    start,
    cr, 
    column_labels,
    expect_comment_or_record,
    expect_record,
    end_record,
    no_more_records,
    comment,
    between_values,
    quoted_string,
    unquoted_string,
    before_unquoted_string,
    escaped_value,
    minus, 
    zero,  
    integer,
    fraction,
    exp1,
    exp2,
    exp3,
    before_done,
    before_unquoted_field,
    before_unquoted_field_tail, 
    before_unquoted_field_tail1,
    before_last_unquoted_field,
    before_last_unquoted_field_tail,
    before_unquoted_subfield,
    before_unquoted_subfield_tail,
    before_quoted_subfield,
    before_quoted_subfield_tail,
    before_quoted_field,
    before_quoted_field_tail,
    before_last_quoted_field,
    before_last_quoted_field_tail,
    done
};

struct default_csv_parsing
{
    bool operator()(csv_errc, const ser_context&) noexcept
    {
        return false;
    }
};

namespace detail {

    template <class CharT, class Allocator>
    class m_columns_filter : public basic_json_content_handler<CharT>
    {
    public:
        typedef typename basic_json_content_handler<CharT>::string_view_type string_view_type;
        typedef CharT char_type;
        typedef Allocator allocator_type;
        typedef typename basic_json_options<CharT>::string_type string_type;
        typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<string_type> string_allocator_type;
    public:
        basic_json_content_handler<CharT>* handler_;
    private:
        typedef basic_json<CharT, preserve_order_policy, Allocator> json_type;

        std::vector<string_type, string_allocator_type> column_names_;
        std::vector<json_decoder<json_type>> decoders_;
        size_t column_index_;
        int level_;
    public:

        m_columns_filter() = delete;

        m_columns_filter(basic_json_content_handler<CharT>& handler)
            : handler_(std::addressof(handler)), column_index_(0), level_(0)
        {
        }

        void initialize(const std::vector<string_type, string_allocator_type>& column_names)
        {
            null_ser_context context;
            for (const auto& name : column_names)
            {
                column_names_.push_back(name);
                decoders_.push_back(json_decoder<json_type>());
                decoders_.back().begin_array(semantic_tag::none, context);
            }
            column_index_ = 0;
            level_ = 0;
        }

        void skip_column()
        {
            ++column_index_;
        }

        void do_flush() override
        {
            null_ser_context context;
            handler_->begin_object(semantic_tag::none, context);
            for (size_t i = 0; i < column_names_.size(); ++i)
            {
                handler_->name(column_names_[i], context);
                decoders_[i].end_array(context);
                decoders_[i].flush();
                auto j = decoders_[i].get_result();
                j.dump(*handler_);
            }
            handler_->end_object(context);
            handler_->flush();
        }

        bool do_begin_object(semantic_tag, const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected begin_object"));
        }

        bool do_end_object(const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected end_object"));
        }

        bool do_begin_array(semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].begin_array(tag, context);
                ++level_;
            }
            return true;
        }

        bool do_end_array(const ser_context& context) override
        {
            if (level_ > 0)
            {
                decoders_[column_index_].end_array(context);
                ++column_index_;
                --level_;
            }
            else
            {
                column_index_ = 0;
            }
            return true;
        }

        bool do_name(const string_view_type&, const ser_context&) override
        {
            JSONCONS_THROW(json_runtime_error<std::invalid_argument>("unexpected name"));
        }

        bool do_null_value(semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].null_value(tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_string_value(const string_view_type& value, semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].string_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_byte_string_value(const byte_string_view& value,
                                  semantic_tag tag,
                                  const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].byte_string_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_double_value(double value,
                             semantic_tag tag,
                             const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].double_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_int64_value(int64_t value,
                            semantic_tag tag,
                            const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].int64_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_uint64_value(uint64_t value,
                             semantic_tag tag,
                             const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].uint64_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }

        bool do_bool_value(bool value, semantic_tag tag, const ser_context& context) override
        {
            if (column_index_ < column_names_.size())
            {
                decoders_[column_index_].bool_value(value, tag, context);
                if (level_ == 0)
                {
                    ++column_index_;
                }
            }
            return true;
        }
    };

} // namespace detail

template<class CharT,class Allocator=std::allocator<CharT>>
class basic_csv_parser : public ser_context
{
    typedef basic_string_view<CharT> string_view_type;
    typedef CharT char_type;
    typedef Allocator allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<CharT> char_allocator_type;
    typedef std::basic_string<CharT,std::char_traits<CharT>,char_allocator_type> string_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<string_type> string_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_mode> csv_mode_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_type_info> csv_type_info_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<std::vector<string_type,string_allocator_type>> string_vector_allocator_type;
    typedef typename std::allocator_traits<allocator_type>:: template rebind_alloc<csv_parse_state> csv_parse_state_allocator_type;
    typedef basic_json<CharT,preserve_order_policy,Allocator> json_type;

    static const int default_depth = 3;

    csv_parse_state state_;
    detail::m_columns_filter<CharT,Allocator> m_columns_filter_;
    std::vector<csv_mode,csv_mode_allocator_type> stack_;
    basic_json_content_handler<CharT>* handler_;
    std::function<bool(csv_errc,const ser_context&)> err_handler_;
    unsigned long column_;
    unsigned long line_;
    string_type buffer_;
    int depth_;
    const basic_csv_decode_options<CharT>& options_;
    std::vector<string_type,string_allocator_type> column_names_;
    std::vector<std::vector<string_type,string_allocator_type>,string_vector_allocator_type> column_values_;
    std::vector<csv_type_info,csv_type_info_allocator_type> column_types_;
    std::vector<string_type,string_allocator_type> column_defaults_;
    size_t column_index_;
    size_t level_;
    size_t offset_;
    jsoncons::detail::string_to_double to_double_; 
    const CharT* begin_input_;
    const CharT* input_end_;
    const CharT* input_ptr_;
    bool continue_;
    std::vector<csv_parse_state,csv_parse_state_allocator_type> state_stack_;

public:
    basic_csv_parser(basic_json_content_handler<CharT>& handler)
       : basic_csv_parser(handler, 
                          basic_csv_options<CharT>::get_default_options(), 
                          default_csv_parsing())
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     const basic_csv_decode_options<CharT>& options)
        : basic_csv_parser(handler, 
                           options, 
                           default_csv_parsing())
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     std::function<bool(csv_errc,const ser_context&)> err_handler)
        : basic_csv_parser(handler, basic_csv_options<CharT>::get_default_options(), err_handler)
    {
    }

    basic_csv_parser(basic_json_content_handler<CharT>& handler,
                     const basic_csv_decode_options<CharT>& options,
                     std::function<bool(csv_errc,const ser_context&)> err_handler)
       : m_columns_filter_(handler),
         handler_(options.mapping() == mapping_type::m_columns ? &m_columns_filter_ : std::addressof(handler)),
         err_handler_(err_handler),
         options_(options),
         level_(0),
         offset_(0),
         begin_input_(nullptr),
         input_end_(nullptr),
         input_ptr_(nullptr),
         continue_(true)
         
    {
        depth_ = default_depth;
        state_ = csv_parse_state::start;
        line_ = 1;
        column_ = 1;
        column_index_ = 0;
        stack_.reserve(default_depth);
        reset();
    }

    ~basic_csv_parser()
    {
    }

    bool done() const
    {
        return state_ == csv_parse_state::done;
    }

    bool stopped() const
    {
        return !continue_;
    }

    bool finished() const
    {
        return !continue_;
        //return !continue_ && (state_ != csv_parse_state::no_more_records || state_ != csv_parse_state::before_done);
    }

    bool source_exhausted() const
    {
        return input_ptr_ == input_end_;
    }

    const std::vector<std::basic_string<CharT>>& column_labels() const
    {
        return column_names_;
    }

    void reset()
    {
        stack_.clear();
        column_names_.clear();
        column_types_.clear();
        column_defaults_.clear();

        stack_.push_back(csv_mode::initial);

        for (const auto& name : options_.column_names())
        {
            column_names_.emplace_back(name.data(),name.size());
        }
        for (const auto& name : options_.column_types())
        {
            column_types_.push_back(name);
        }
        for (const auto& name : options_.column_defaults())
        {
            column_defaults_.emplace_back(name.data(), name.size());
        }
        if (options_.header_lines() > 0)
        {
            stack_.push_back(csv_mode::header);
        }
        else
        {
            stack_.push_back(csv_mode::data);
        }
        state_ = csv_parse_state::start;
        column_index_ = 0;
        column_ = 1;
        level_ = 0;
    }

    void restart()
    {
        continue_ = true;
    }

    void parse_some(basic_json_content_handler<CharT>& handler)
    {
        std::error_code ec;
        parse_some(handler,ec);
        if (ec)
        {
            throw ser_error(ec,line_,column_);
        }
    }

    void parse_some(basic_json_content_handler<CharT>& handler, std::error_code& ec)
    {
        if (options_.mapping() == mapping_type::m_columns)
        {
            m_columns_filter_.handler_ = std::addressof(handler);
        }
        else
        {
            handler_ = std::addressof(handler);
        }

        const CharT* local_input_end = input_end_;

        if (input_ptr_ == local_input_end && continue_)
        {
            switch (state_)
            {
                case csv_parse_state::before_unquoted_field:
                case csv_parse_state::before_last_unquoted_field:
                case csv_parse_state::before_last_unquoted_field_tail:
                    end_unquoted_string_value();
                    if (stack_.back() == csv_mode::subfields)
                    {
                        stack_.pop_back();
                        continue_ = handler_->end_array(*this);
                    }
                    ++column_index_;
                    state_ = csv_parse_state::end_record;
                    break;
                case csv_parse_state::before_unquoted_string: 
                    buffer_.clear();
                    JSONCONS_FALLTHROUGH;
                case csv_parse_state::unquoted_string: 
                    if (options_.trim_leading() || options_.trim_trailing())
                    {
                        trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                    }
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        before_value();
                        state_ = csv_parse_state::before_unquoted_field;
                    }
                    else
                    {
                        if (options_.trim_leading() || options_.trim_trailing())
                        {
                            trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                        }
                        if (!(options_.ignore_empty_values() && buffer_.empty()))
                        {
                            before_value();
                            state_ = csv_parse_state::before_last_quoted_field;
                        }
                        else
                        {
                            state_ = csv_parse_state::end_record;
                        }
                        state_ = csv_parse_state::end_record;
                    }
                    break;
                case csv_parse_state::before_last_quoted_field:
                    end_quoted_string_value();
                    ++column_index_;
                    state_ = csv_parse_state::end_record;
                    break;
                case csv_parse_state::escaped_value:
                    if (options_.quote_escape_char() == options_.quote_char())
                    {
                        if (!(options_.ignore_empty_values() && buffer_.empty()))
                        {
                            before_value();
                            ++column_;
                            state_ = csv_parse_state::before_last_quoted_field;
                        }
                        else
                        {
                            state_ = csv_parse_state::end_record;
                        }
                    }
                    else
                    {
                        ec = csv_errc::invalid_escaped_char;
                        continue_ = false;
                        return;
                    }
                    break;
                case csv_parse_state::end_record:
                    if (column_index_ > 0)
                    {
                        after_record();
                    }
                    state_ = csv_parse_state::no_more_records;
                    break;
                case csv_parse_state::no_more_records: 
                    switch (stack_.back()) 
                    {
                        case csv_mode::header:
                            stack_.pop_back();
                            break;
                        case csv_mode::data:
                            stack_.pop_back();
                            break;
                        default:
                            break;
                    }
                    continue_ = handler_->end_array(*this);
                    state_ = csv_parse_state::before_done;
                    break;
                case csv_parse_state::before_done:
                    if (!(stack_.size() == 1 && stack_.back() == csv_mode::initial))
                    {
                        err_handler_(csv_errc::unexpected_eof, *this);
                        ec = csv_errc::unexpected_eof;
                        continue_ = false;
                        return;
                    }
                    stack_.pop_back();
                    handler_->flush();
                    state_ = csv_parse_state::done;
                    continue_ = false;
                    return;
                    break;
                default:
                    state_ = csv_parse_state::end_record;
                    break;
            }
        }

        for (; (input_ptr_ < local_input_end) && continue_;)
        {
            CharT curr_char = *input_ptr_;

            switch (state_) 
            {
                case csv_parse_state::cr:
                    ++line_;
                    column_ = 1;
                    switch (*input_ptr_)
                    {
                        case '\n':
                            ++input_ptr_;
                            state_ = pop_state();
                            break;
                        default:
                            state_ = pop_state();
                            break;
                    }
                    break;
                case csv_parse_state::start:
                    if (options_.mapping() != mapping_type::m_columns)
                    {
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                    }
                    if (!options_.assume_header() && options_.mapping() == mapping_type::n_rows && options_.column_names().size() > 0)
                    {
                        column_index_ = 0;
                        state_ = csv_parse_state::column_labels;
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                    }
                    else
                    {
                        state_ = csv_parse_state::expect_comment_or_record;
                    }
                    break;
                case csv_parse_state::column_labels:
                    if (column_index_ < column_names_.size())
                    {
                        continue_ = handler_->string_value(column_names_[column_index_], semantic_tag::none, *this);
                        ++column_index_;
                    }
                    else
                    {
                        continue_ = handler_->end_array(*this);
                        state_ = csv_parse_state::expect_comment_or_record; 
                        //stack_.back() = csv_mode::data;
                        column_index_ = 0;
                    }
                    break;
                case csv_parse_state::comment: 
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            break;
                        }
                        case '\r':
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            push_state(state_);
                            state_ = csv_parse_state::cr;
                            break;
                        default:
                            ++column_;
                            break;
                    }
                    ++input_ptr_;
                    break;
                
                case csv_parse_state::expect_comment_or_record:
                    buffer_.clear();
                    if (curr_char == options_.comment_starter())
                    {
                        state_ = csv_parse_state::comment;
                        ++column_;
                        ++input_ptr_;
                    }
                    else
                    {
                        state_ = csv_parse_state::expect_record;
                    }
                    break;
                case csv_parse_state::quoted_string: 
                    {
                        if (curr_char == options_.quote_escape_char())
                        {
                            state_ = csv_parse_state::escaped_value;
                        }
                        else if (curr_char == options_.quote_char())
                        {
                            state_ = csv_parse_state::between_values;
                        }
                        else
                        {
                            buffer_.push_back(static_cast<CharT>(curr_char));
                        }
                    }
                    ++column_;
                    ++input_ptr_;
                    break;
                case csv_parse_state::escaped_value: 
                    {
                        if (curr_char == options_.quote_char())
                        {
                            buffer_.push_back(static_cast<CharT>(curr_char));
                            state_ = csv_parse_state::quoted_string;
                            ++column_;
                            ++input_ptr_;
                        }
                        else if (options_.quote_escape_char() == options_.quote_char())
                        {
                            state_ = csv_parse_state::between_values;
                        }
                        else
                        {
                            ec = csv_errc::invalid_escaped_char;
                            continue_ = false;
                            return;
                        }
                    }
                    break;
                case csv_parse_state::between_values:
                    switch (curr_char)
                    {
                        case '\r':
                        case '\n':
                        {
                            if (options_.trim_leading() || options_.trim_trailing())
                            {
                                trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                            }
                            if (!(options_.ignore_empty_values() && buffer_.empty()))
                            {
                                before_value();
                                state_ = csv_parse_state::before_last_quoted_field;
                            }
                            else
                            {
                                state_ = csv_parse_state::end_record;
                            }
                            break;
                        }
                        default:
                            if (curr_char == options_.field_delimiter())
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                before_value();
                                state_ = csv_parse_state::before_quoted_field;
                            }
                            else if ((options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first))
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                before_value();
                                state_ = csv_parse_state::before_quoted_subfield;
                            }
                            break;
                    }
                    break;
                case csv_parse_state::before_unquoted_string: 
                {
                    buffer_.clear();
                    state_ = csv_parse_state::unquoted_string;
                    break;
                }
                case csv_parse_state::before_unquoted_field:
                    end_unquoted_string_value();
                    state_ = csv_parse_state::before_unquoted_field_tail;
                    break;
                case csv_parse_state::before_unquoted_field_tail:
                {
                    if (stack_.back() == csv_mode::subfields)
                    {
                        stack_.pop_back();
                        continue_ = handler_->end_array(*this);
                    }
                    ++column_index_;
                    state_ = csv_parse_state::before_unquoted_string;
                    ++column_;
                    ++input_ptr_;
                    break;
                }
                case csv_parse_state::before_unquoted_field_tail1:
                {
                    if (stack_.back() == csv_mode::subfields)
                    {
                        stack_.pop_back();
                        continue_ = handler_->end_array(*this);
                    }
                    state_ = csv_parse_state::end_record;
                    ++column_;
                    ++input_ptr_;
                    break;
                }

                case csv_parse_state::before_last_unquoted_field:
                    end_unquoted_string_value();
                    state_ = csv_parse_state::before_last_unquoted_field_tail;
                    break;

                case csv_parse_state::before_last_unquoted_field_tail:
                    if (stack_.back() == csv_mode::subfields)
                    {
                        stack_.pop_back();
                        continue_ = handler_->end_array(*this);
                    }
                    ++column_index_;
                    state_ = csv_parse_state::end_record;
                    break;

                case csv_parse_state::before_unquoted_subfield:
                    if (stack_.back() == csv_mode::data)
                    {
                        stack_.push_back(csv_mode::subfields);
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                    }
                    state_ = csv_parse_state::before_unquoted_subfield_tail;
                    break; 
                case csv_parse_state::before_unquoted_subfield_tail:
                    end_unquoted_string_value();
                    state_ = csv_parse_state::before_unquoted_string;
                    ++column_;
                    ++input_ptr_;
                    break;
                case csv_parse_state::before_quoted_field:
                    end_quoted_string_value();
                    state_ = csv_parse_state::before_unquoted_field_tail; // return to unquoted
                    break;
                case csv_parse_state::before_quoted_subfield:
                    if (stack_.back() == csv_mode::data)
                    {
                        stack_.push_back(csv_mode::subfields);
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                    }
                    state_ = csv_parse_state::before_quoted_subfield_tail;
                    break; 
                case csv_parse_state::before_quoted_subfield_tail:
                    end_quoted_string_value();
                    state_ = csv_parse_state::before_unquoted_string;
                    ++column_;
                    ++input_ptr_;
                    break;
                case csv_parse_state::before_last_quoted_field:
                    end_quoted_string_value();
                    state_ = csv_parse_state::before_last_quoted_field_tail;
                    break;
                case csv_parse_state::before_last_quoted_field_tail:
                    if (stack_.back() == csv_mode::subfields)
                    {
                        stack_.pop_back();
                        continue_ = handler_->end_array(*this);
                    }
                    ++column_index_;
                    state_ = csv_parse_state::end_record;
                    break;
                case csv_parse_state::unquoted_string: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        case '\r':
                        {
                            if (options_.trim_leading() || options_.trim_trailing())
                            {
                                trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                            }
                            if (!(options_.ignore_empty_values() && buffer_.empty()))
                            {
                                before_value();
                                state_ = csv_parse_state::before_last_unquoted_field;
                            }
                            else
                            {
                                state_ = csv_parse_state::end_record;
                            }
                            break;
                        }
                        default:
                            if (curr_char == options_.field_delimiter())
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                before_value();
                                state_ = csv_parse_state::before_unquoted_field;
                            }
                            else if (options_.subfield_delimiter().second && curr_char == options_.subfield_delimiter().first)
                            {
                                if (options_.trim_leading() || options_.trim_trailing())
                                {
                                    trim_string_buffer(options_.trim_leading(),options_.trim_trailing());
                                }
                                before_value();
                                state_ = csv_parse_state::before_unquoted_subfield;
                            }
                            else if (curr_char == options_.quote_char())
                            {
                                buffer_.clear();
                                state_ = csv_parse_state::quoted_string;
                                ++column_;
                                ++input_ptr_;
                            }
                            else
                            {
                                buffer_.push_back(static_cast<CharT>(curr_char));
                                ++column_;
                                ++input_ptr_;
                            }
                            break;
                    }
                    break;
                }
                case csv_parse_state::expect_record: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            if (!options_.ignore_empty_lines())
                            {
                                before_record();
                                state_ = csv_parse_state::end_record;
                            }
                            else
                            {
                                ++line_;
                                column_ = 1;
                                state_ = csv_parse_state::expect_comment_or_record;
                                ++input_ptr_;
                            }
                            break;
                        }
                        case '\r':
                            if (!options_.ignore_empty_lines())
                            {
                                before_record();
                                state_ = csv_parse_state::end_record;
                            }
                            else
                            {
                                ++line_;
                                column_ = 1;
                                state_ = csv_parse_state::expect_comment_or_record;
                                ++input_ptr_;
                                push_state(state_);
                                state_ = csv_parse_state::cr;
                            }
                            break;
                        case ' ':
                        case '\t':
                            if (!options_.trim_leading())
                            {
                                buffer_.push_back(static_cast<CharT>(curr_char));
                                before_record();
                                state_ = csv_parse_state::unquoted_string;
                            }
                            ++column_;
                            ++input_ptr_;
                            break;
                        default:
                            before_record();
                            if (curr_char == options_.quote_char())
                            {
                                buffer_.clear();
                                state_ = csv_parse_state::quoted_string;
                                ++column_;
                                ++input_ptr_;
                            }
                            else
                            {
                                state_ = csv_parse_state::unquoted_string;
                            }
                            break;
                        }
                    break;
                    }
                case csv_parse_state::end_record: 
                {
                    switch (curr_char)
                    {
                        case '\n':
                        {
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            after_record();
                            ++input_ptr_;
                            break;
                        }
                        case '\r':
                            ++line_;
                            column_ = 1;
                            state_ = csv_parse_state::expect_comment_or_record;
                            after_record();
                            push_state(state_);
                            state_ = csv_parse_state::cr;
                            ++input_ptr_;
                            break;
                        case ' ':
                        case '\t':
                            ++column_;
                            ++input_ptr_;
                            break;
                        default:
                            err_handler_(csv_errc::invalid_csv_text, *this);
                            ec = csv_errc::invalid_csv_text;
                            continue_ = false;
                            return;
                        }
                    break;
                }
                default:
                    err_handler_(csv_errc::invalid_state, *this);
                    ec = csv_errc::invalid_state;
                    continue_ = false;
                    return;
            }
            if (line_ > options_.max_lines())
            {
                state_ = csv_parse_state::done;
                continue_ = false;
            }
        }
    }

    void finish_parse()
    {
        std::error_code ec;
        finish_parse(ec);
        if (ec)
        {
            throw ser_error(ec,line_,column_);
        }
    }

    void finish_parse(std::error_code& ec)
    {
        while (continue_)
        {
            parse_some(ec);
        }
    }

    csv_parse_state state() const
    {
        return state_;
    }

    void update(const string_view_type sv)
    {
        update(sv.data(),sv.length());
    }

    void update(const CharT* data, size_t length)
    {
        begin_input_ = data;
        input_end_ = data + length;
        input_ptr_ = begin_input_;
    }

    size_t line() const override
    {
        return line_;
    }

    size_t column() const override
    {
        return column_;
    }
private:
    // name
    void before_value()
    {
        switch (stack_.back())
        {
            case csv_mode::header:
                if (options_.trim_leading_inside_quotes() | options_.trim_trailing_inside_quotes())
                {
                    trim_string_buffer(options_.trim_leading_inside_quotes(),options_.trim_trailing_inside_quotes());
                }
                if (options_.assume_header() && line_ == 1)
                {
                    column_names_.push_back(buffer_);
                    if (options_.mapping() == mapping_type::n_rows)
                    {
                        continue_ = handler_->string_value(buffer_, semantic_tag::none, *this);
                    }
                }
                break;
            case csv_mode::data:
                if (options_.mapping() == mapping_type::n_objects)
                {
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        if (column_index_ < column_names_.size() + offset_)
                        {
                            continue_ = handler_->name(column_names_[column_index_ - offset_], *this);
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    // begin_array or begin_record
    void before_record()
    {
        offset_ = 0;

        switch (stack_.back())
        {
            case csv_mode::header:
                if (options_.assume_header() && line_ == 1)
                {
                    if (options_.mapping() == mapping_type::n_rows)
                    {
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                    }
                }
                break;
            case csv_mode::data:
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        continue_ = handler_->begin_array(semantic_tag::none, *this);
                        break;
                    case mapping_type::n_objects:
                        continue_ = handler_->begin_object(semantic_tag::none, *this);
                        break;
                    case mapping_type::m_columns:
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    // end_array, begin_array, string_value (headers)
    void after_record()
    {
        if (column_types_.size() > 0)
        {
            if (level_ > 0)
            {
                continue_ = handler_->end_array(*this);
                level_ = 0;
            }
        }
        switch (stack_.back())
        {
            case csv_mode::header:
                if (line_ >= options_.header_lines())
                {
                    stack_.back() = csv_mode::data;
                }
                column_values_.resize(column_names_.size());
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        if (options_.assume_header())
                        {
                            continue_ = handler_->end_array(*this);
                        }
                        break;
                    case mapping_type::m_columns:
                        m_columns_filter_.initialize(column_names_);
                        break;
                    default:
                        break;
                }
                break;
            case csv_mode::data:
            case csv_mode::subfields:
            {
                switch (options_.mapping())
                {
                    case mapping_type::n_rows:
                        continue_ = handler_->end_array(*this);
                        break;
                    case mapping_type::n_objects:
                        continue_ = handler_->end_object(*this);
                        break;
                    case mapping_type::m_columns:
                        continue_ = handler_->end_array(*this);
                        break;
                }
                break;
            }
            default:
                break;
        }
        column_index_ = 0;
    }

    void trim_string_buffer(bool trim_leading, bool trim_trailing)
    {
        size_t start = 0;
        size_t length = buffer_.length();
        if (trim_leading)
        {
            bool done = false;
            while (!done && start < buffer_.length())
            {
                if ((buffer_[start] < 256) && std::isspace(buffer_[start]))
                {
                    ++start;
                }
                else
                {
                    done = true;
                }
            }
        }
        if (trim_trailing)
        {
            bool done = false;
            while (!done && length > 0)
            {
                if ((buffer_[length-1] < 256) && std::isspace(buffer_[length-1]))
                {
                    --length;
                }
                else
                {
                    done = true;
                }
            }
        }
        if (start != 0 || length != buffer_.size())
        {
            buffer_ = buffer_.substr(start,length-start);
        }
    }

    /*
        end_array, begin_array, xxx_value (end_value)
    */
    void end_unquoted_string_value() 
    {
        switch (stack_.back())
        {
            case csv_mode::data:
            case csv_mode::subfields:
                switch (options_.mapping())
                {
                case mapping_type::n_rows:
                    if (options_.unquoted_empty_value_is_null() && buffer_.length() == 0)
                    {
                        continue_ = handler_->null_value(semantic_tag::none, *this);
                    }
                    else
                    {
                        end_value(options_.infer_types());
                    }
                    break;
                case mapping_type::n_objects:
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        if (column_index_ < column_names_.size() + offset_)
                        {
                            if (options_.unquoted_empty_value_is_null() && buffer_.length() == 0)
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(options_.infer_types());
                            }
                        }
                        else if (level_ > 0)
                        {
                            if (options_.unquoted_empty_value_is_null() && buffer_.length() == 0)
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(options_.infer_types());
                            }
                        }
                    }
                    break;
                case mapping_type::m_columns:
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        end_value(options_.infer_types());
                    }
                    else
                    {
                        m_columns_filter_.skip_column();
                    }
                    break;
                }
                break;
            default:
                break;
        }
    }

    void end_quoted_string_value() 
    {
        switch (stack_.back())
        {
            case csv_mode::data:
            case csv_mode::subfields:
                if (options_.trim_leading_inside_quotes() | options_.trim_trailing_inside_quotes())
                {
                    trim_string_buffer(options_.trim_leading_inside_quotes(),options_.trim_trailing_inside_quotes());
                }
                switch (options_.mapping())
                {
                case mapping_type::n_rows:
                    end_value(false);
                    break;
                case mapping_type::n_objects:
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        if (column_index_ < column_names_.size() + offset_)
                        {
                            if (options_.unquoted_empty_value_is_null() && buffer_.length() == 0)
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                            else 
                            {
                                end_value(false);
                            }
                        }
                        else if (level_ > 0)
                        {
                            if (options_.unquoted_empty_value_is_null() && buffer_.length() == 0)
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                            else
                            {
                                end_value(false);
                            }
                        }
                    }
                    break;
                case mapping_type::m_columns:
                    if (!(options_.ignore_empty_values() && buffer_.empty()))
                    {
                        end_value(options_.infer_types());
                    }
                    else
                    {
                        m_columns_filter_.skip_column();
                    }
                    break;
                }
                break;
            default:
                break;
        }
    }

    void end_value(bool infer_types)
    {
        if (column_index_ < column_types_.size() + offset_)
        {
            if (column_types_[column_index_ - offset_].col_type == csv_column_type::repeat_t)
            {
                offset_ = offset_ + column_types_[column_index_ - offset_].rep_count;
                if (column_index_ - offset_ + 1 < column_types_.size())
                {
                    if (column_index_ == offset_ || level_ > column_types_[column_index_-offset_].level)
                    {
                        continue_ = handler_->end_array(*this);
                    }
                    level_ = column_index_ == offset_ ? 0 : column_types_[column_index_ - offset_].level;
                }
            }
            if (level_ < column_types_[column_index_ - offset_].level)
            {
                continue_ = handler_->begin_array(semantic_tag::none, *this);
                level_ = column_types_[column_index_ - offset_].level;
            }
            else if (level_ > column_types_[column_index_ - offset_].level)
            {
                continue_ = handler_->end_array(*this);
                level_ = column_types_[column_index_ - offset_].level;
            }
            switch (column_types_[column_index_ - offset_].col_type)
            {
                case csv_column_type::integer_t:
                    {
                        std::istringstream iss{ std::string(buffer_) };
                        int64_t val;
                        iss >> val;
                        if (!iss.fail())
                        {
                            continue_ = handler_->int64_value(val, semantic_tag::none, *this);
                        }
                        else
                        {
                            if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                            {
                                basic_json_parser<CharT> parser;
                                parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                parser.parse_some(*handler_);
                                parser.finish_parse(*handler_);
                            }
                            else
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                        }
                    }
                    break;
                case csv_column_type::float_t:
                    {
                        if (options_.lossless_number())
                        {
                            continue_ = handler_->string_value(buffer_,semantic_tag::bigdec, *this);
                        }
                        else
                        {
                            std::istringstream iss{ std::string(buffer_) };
                            double val;
                            iss >> val;
                            if (!iss.fail())
                            {
                                continue_ = handler_->double_value(val, semantic_tag::none, *this);
                            }
                            else
                            {
                                if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                                {
                                    basic_json_parser<CharT> parser;
                                    parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                    parser.parse_some(*handler_);
                                    parser.finish_parse(*handler_);
                                }
                                else
                                {
                                    continue_ = handler_->null_value(semantic_tag::none, *this);
                                }
                            }
                        }
                    }
                    break;
                case csv_column_type::boolean_t:
                    {
                        if (buffer_.length() == 1 && buffer_[0] == '0')
                        {
                            continue_ = handler_->bool_value(false, semantic_tag::none, *this);
                        }
                        else if (buffer_.length() == 1 && buffer_[0] == '1')
                        {
                            continue_ = handler_->bool_value(true, semantic_tag::none, *this);
                        }
                        else if (buffer_.length() == 5 && ((buffer_[0] == 'f' || buffer_[0] == 'F') && (buffer_[1] == 'a' || buffer_[1] == 'A') && (buffer_[2] == 'l' || buffer_[2] == 'L') && (buffer_[3] == 's' || buffer_[3] == 'S') && (buffer_[4] == 'e' || buffer_[4] == 'E')))
                        {
                            continue_ = handler_->bool_value(false, semantic_tag::none, *this);
                        }
                        else if (buffer_.length() == 4 && ((buffer_[0] == 't' || buffer_[0] == 'T') && (buffer_[1] == 'r' || buffer_[1] == 'R') && (buffer_[2] == 'u' || buffer_[2] == 'U') && (buffer_[3] == 'e' || buffer_[3] == 'E')))
                        {
                            continue_ = handler_->bool_value(true, semantic_tag::none, *this);
                        }
                        else
                        {
                            if (column_index_ - offset_ < column_defaults_.size() && column_defaults_[column_index_ - offset_].length() > 0)
                            {
                                basic_json_parser<CharT> parser;
                                parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                                parser.parse_some(*handler_);
                                parser.finish_parse(*handler_);
                            }
                            else
                            {
                                continue_ = handler_->null_value(semantic_tag::none, *this);
                            }
                        }
                    }
                    break;
                default:
                    if (buffer_.length() > 0)
                    {
                        continue_ = handler_->string_value(buffer_, semantic_tag::none, *this);
                    }
                    else
                    {
                        if (column_index_ < column_defaults_.size() + offset_ && column_defaults_[column_index_ - offset_].length() > 0)
                        {
                            basic_json_parser<CharT> parser;
                            parser.update(column_defaults_[column_index_ - offset_].data(),column_defaults_[column_index_ - offset_].length());
                            parser.parse_some(*handler_);
                            parser.finish_parse(*handler_);
                        }
                        else
                        {
                            continue_ = handler_->string_value(string_view_type(), semantic_tag::none, *this);
                        }
                    }
                    break;  
            }
        }
        else
        {
            if (infer_types)
            {
                end_value_with_numeric_check();
            }
            else
            {
                continue_ = handler_->string_value(buffer_, semantic_tag::none, *this);
            }
        }
    }

    enum class numeric_check_state 
    {
        initial,
        null,
        boolean_true,
        boolean_false,
        minus,
        zero,
        integer,
        fraction1,
        fraction,
        exp1,
        exp,
        done
    };

    /*
        xxx_value 
    */
    void end_value_with_numeric_check()
    {
        numeric_check_state state = numeric_check_state::initial;
        bool is_negative = false;
        int precision = 0;
        uint8_t decimal_places = 0;

        auto last = buffer_.end();

        std::string buffer;
        for (auto p = buffer_.begin(); state != numeric_check_state::done && p != last; ++p)
        {
            switch (state)
            {
                case numeric_check_state::initial:
                {
                    switch (*p)
                    {
                    case 'n':case 'N':
                        if ((last-p) == 4 && (p[1] == 'u' || p[1] == 'U') && (p[2] == 'l' || p[2] == 'L') && (p[3] == 'l' || p[3] == 'L'))
                        {
                            state = numeric_check_state::null;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case 't':case 'T':
                        if ((last-p) == 4 && (p[1] == 'r' || p[1] == 'R') && (p[2] == 'u' || p[2] == 'U') && (p[3] == 'e' || p[3] == 'U'))
                        {
                            state = numeric_check_state::boolean_true;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case 'f':case 'F':
                        if ((last-p) == 5 && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'l' || p[2] == 'L') && (p[3] == 's' || p[3] == 'S') && (p[4] == 'e' || p[4] == 'E'))
                        {
                            state = numeric_check_state::boolean_false;
                        }
                        else
                        {
                            state = numeric_check_state::done;
                        }
                        break;
                    case '-':
                        is_negative = true;
                        buffer.push_back(*p);
                        state = numeric_check_state::minus;
                        break;
                    case '0':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::zero:
                {
                    switch (*p)
                    {
                    case '.':
                        buffer.push_back(to_double_.get_decimal_point());
                        state = numeric_check_state::fraction1;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::integer:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        break;
                    case '.':
                        buffer.push_back(to_double_.get_decimal_point());
                        state = numeric_check_state::fraction1;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::minus:
                {
                    switch (*p)
                    {
                    case '0':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::zero;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::fraction1:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        ++decimal_places;
                        buffer.push_back(*p);
                        state = numeric_check_state::fraction;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::fraction:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        ++precision;
                        ++decimal_places;
                        buffer.push_back(*p);
                        break;
                    case 'e':case 'E':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp1;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::exp1:
                {
                    switch (*p)
                    {
                    case '-':
                        buffer.push_back(*p);
                        state = numeric_check_state::exp;
                        break;
                    case '+':
                        state = numeric_check_state::exp;
                        break;
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        buffer.push_back(*p);
                        state = numeric_check_state::integer;
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                case numeric_check_state::exp:
                {
                    switch (*p)
                    {
                    case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        buffer.push_back(*p);
                        break;
                    default:
                        state = numeric_check_state::done;
                        break;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        switch (state)
        {
            case numeric_check_state::null:
                continue_ = handler_->null_value(semantic_tag::none, *this);
                break;
            case numeric_check_state::boolean_true:
                continue_ = handler_->bool_value(true, semantic_tag::none, *this);
                break;
            case numeric_check_state::boolean_false:
                continue_ = handler_->bool_value(false, semantic_tag::none, *this);
                break;
            case numeric_check_state::zero:
            case numeric_check_state::integer:
            {
                if (is_negative)
                {
                    auto result = jsoncons::detail::to_integer<int64_t>(buffer_.data(), buffer_.length());
                    if (result.ec == jsoncons::detail::to_integer_errc())
                    {
                        continue_ = handler_->int64_value(result.value, semantic_tag::none, *this);
                    }
                    else // Must be overflow
                    {
                        continue_ = handler_->string_value(buffer_, semantic_tag::bigint, *this);
                    }
                }
                else
                {
                    auto result = jsoncons::detail::to_integer<uint64_t>(buffer_.data(), buffer_.length());
                    if (result.ec == jsoncons::detail::to_integer_errc())
                    {
                        continue_ = handler_->uint64_value(result.value, semantic_tag::none, *this);
                    }
                    else if (result.ec == jsoncons::detail::to_integer_errc::overflow)
                    {
                        continue_ = handler_->string_value(buffer_, semantic_tag::bigint, *this);
                    }
                    else
                    {
                        JSONCONS_THROW(json_runtime_error<std::invalid_argument>(make_error_code(result.ec).message()));
                    }
                }
                break;
            }
            case numeric_check_state::fraction:
            case numeric_check_state::exp:
            {
                if (options_.lossless_number())
                {
                    continue_ = handler_->string_value(buffer_,semantic_tag::bigdec, *this);
                }
                else
                {
                    double d = to_double_(buffer.c_str(), buffer.length());
                    continue_ = handler_->double_value(d, semantic_tag::none, *this);
                }
                break;
            }
            default:
            {
                continue_ = handler_->string_value(buffer_, semantic_tag::none, *this);
                break;
            }
        }
    } 

    void push_state(csv_parse_state state)
    {
        state_stack_.push_back(state);
    }

    csv_parse_state pop_state()
    {
        JSONCONS_ASSERT(!state_stack_.empty())
        csv_parse_state state = state_stack_.back();
        state_stack_.pop_back();
        return state;
    }
};

typedef basic_csv_parser<char> csv_parser;
typedef basic_csv_parser<wchar_t> wcsv_parser;

}}

#endif

