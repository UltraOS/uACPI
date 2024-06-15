#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <iostream>

class ArgParser
{
public:
    using help_callback = std::function<void()>;

    ArgParser& add_param(
        std::string_view full_arg, char short_arg, std::string_view description,
        bool optional = true
    )
    {
        return add_custom(full_arg, short_arg, ArgType::param, description,
                          optional);
    }

    ArgParser& add_flag(
        std::string_view full_arg, char short_arg, std::string_view description,
        bool optional = true
    )
    {
        return add_custom(full_arg, short_arg, ArgType::flag, description,
                          optional);
    }

    ArgParser& add_list(
        std::string_view full_arg, char short_arg, std::string_view description,
        bool optional = true
    )
    {
        return add_custom(full_arg, short_arg, ArgType::list, description,
                          optional);
    }

    ArgParser& add_help(
        std::string_view full_arg, char short_arg,
        std::string_view description, help_callback on_help_requested
    )
    {
        m_help_callback = on_help_requested;
        return add_custom(full_arg, short_arg, ArgType::help, description, true);
    }

    ArgParser& add_positional(
        std::string_view name, std::string_view description)
    {
        if (m_num_positional_args != m_args.size()) {
            throw std::runtime_error(
                "positional argument follows keyword argument"
            );
        }

        m_num_positional_args++;
        return add_custom(name, '\0', ArgType::positional, description, false);
    }

    void parse(int argc, char** argv)
    {
        size_t num_args = argc;

        if (num_args < 2) {
            if (m_help_callback)
                m_help_callback();

            std::exit(1);
        }

        if (m_num_positional_args) {
            if ((num_args - 1) < m_num_positional_args) {
                throw std::runtime_error(
                    "expected at least " + std::to_string(m_num_positional_args)
                    + "positional arguments"
                );
            }

            for (auto i = 0; m_parsed_args.size() < m_num_positional_args; ++i)
                m_parsed_args[m_args[i].as_full].emplace_back(argv[1 + i]);
        }

        const ArgSpec* active_spec = nullptr;

        for (auto arg_index = 1 + m_num_positional_args; arg_index < num_args;
             ++arg_index) {
            auto* current_arg = argv[arg_index];
            auto is_new_arg = is_arg(current_arg);

            if (active_spec) {
                if (!is_new_arg) {
                    if (active_spec->is_flag()) {
                        throw std::runtime_error(
                            std::string("unexpected argument ") + current_arg
                        );
                    }

                    if (active_spec->is_param() &&
                        m_parsed_args[active_spec->as_full].size() == 1) {
                        throw std::runtime_error(
                            "too many arguments for " + active_spec->as_full
                        );
                    }

                    m_parsed_args[active_spec->as_full].emplace_back(
                        current_arg
                    );
                    continue;
                }

                if ((active_spec->is_param() || active_spec->is_list()) &&
                    m_parsed_args[active_spec->as_full].empty()) {
                    throw std::runtime_error(
                        "expected an argument for " + active_spec->as_full
                    );
                }
            }

            auto as_full_arg = extract_full_arg(current_arg);
            if (as_full_arg.empty()) {
                throw std::runtime_error(
                    std::string("unexpected argument ") + current_arg
                );
            }

            active_spec = &arg_spec_of(as_full_arg);
            if (active_spec->is_help()) {
                if (m_help_callback)
                    m_help_callback();

                std::exit(1);
            }

            m_parsed_args[as_full_arg];
            continue;
        }

        ensure_mandatory_args_are_satisfied();
    }

    const std::vector<std::string>& get_list(std::string_view arg) const
    {
        ensure_arg_is_parsed(arg);

        return m_parsed_args.at(std::string(arg));
    }

    const std::vector<std::string>& get_list_or(
        std::string_view arg, const std::vector<std::string>& default_value
    ) const
    {
        if (is_arg_parsed(arg))
            return get_list(arg);

        return default_value;
    }

    const std::string& get(std::string_view arg) const
    {
        return get_list(arg)[0];
    }

    const std::string& get_or(
        std::string_view arg, const std::string& default_value
    ) const
    {
        if (is_arg_parsed(arg))
            return get(arg);

        return default_value;
    }

    uint64_t get_uint(std::string_view arg) const
    {
        return std::stoull(get(arg));
    }

    uint64_t get_uint_or(std::string_view arg, uint64_t default_value) const
    {
        if (is_arg_parsed(arg))
            return get_uint(arg);

        return default_value;
    }

    int64_t get_int(std::string_view arg) const
    {
        return std::stoll(get(arg));
    }

    int64_t get_int_or(std::string_view arg, int64_t default_value) const
    {
        if (is_arg_parsed(arg))
            return get_int(arg);

        return default_value;
    }

    bool is_set(std::string_view arg) const
    {
        return is_arg_parsed(arg);
    }

    const std::vector<std::string>& get_list(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_list(arg_spec.as_full);
    }

    std::string_view get(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get(arg_spec.as_full);
    }

    uint64_t get_uint(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_uint(arg_spec.as_full);
    }

    uint64_t get_int(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return get_int(arg_spec.as_full);
    }

    bool is_set(char arg) const
    {
        const auto& arg_spec = arg_spec_of(arg);

        return is_set(arg_spec.as_full);
    }

    std::ostream& print(std::ostream& stream = std::cout) const
    {
        for (size_t i = 0; i < m_num_positional_args; ++i) {
            auto& arg = m_args[i];
            stream << "           [" << arg.as_full << "] " << arg.description
                   << std::endl;
        }

        for (size_t i = m_num_positional_args; i < m_args.size(); ++i) {
            auto& arg = m_args[i];

            stream << (arg.is_optional ? "(optional) " : "           ")
                   << "[--"
                   << arg.as_full
                   << "/-"
                   << arg.as_short
                   << "] "
                   << arg.description
                   << std::endl;
        }

        return stream;
    }

    friend std::ostream& operator<<(
        std::ostream& stream, const ArgParser& argp
    ) {
        return argp.print(stream);
    }

private:
    enum class ArgType
    {
        flag,
        param,
        list,
        help,
        positional,
    };

    struct ArgSpec
    {
        std::string as_full;
        char        as_short;
        ArgType     type;
        std::string description;
        bool        is_optional;

        [[nodiscard]] bool is_list()  const { return type == ArgType::list; }
        [[nodiscard]] bool is_param() const { return type == ArgType::param; }
        [[nodiscard]] bool is_flag()  const { return type == ArgType::flag; }
        [[nodiscard]] bool is_help()  const { return type == ArgType::help; }
    };

    void ensure_mandatory_args_are_satisfied() const
    {
        for (const auto& arg : m_args) {
            if (arg.is_optional)
                continue;

            if (!m_parsed_args.count(arg.as_full))
                throw std::runtime_error(
                    "expected a non-optional argument --" + arg.as_full
                );
        }
    }

    ArgParser& add_custom(
        std::string_view full_arg, char short_arg, ArgType type,
        std::string_view description, bool optional
    )
    {
        m_args.emplace_back(ArgSpec {
            std::string(full_arg),
            short_arg,
            type,
            std::string(description),
            optional,
        });

        return *this;
    }

    bool is_arg_parsed(std::string_view arg) const
    {
        return m_parsed_args.count(std::string(arg));
    }

    void ensure_arg_is_parsed(std::string_view arg) const
    {
        if (!is_arg_parsed(arg)) {
            throw std::runtime_error(
                "couldn't find argument " + std::string(arg)
            );
        }
    }

    const ArgSpec& arg_spec_of(std::string_view arg) const
    {
        auto arg_itr = std::find_if(
            m_args.begin() + m_num_positional_args,
            m_args.end(),
            [&](const ArgSpec& my_arg) {
                return my_arg.as_full == arg;
            }
        );

        if (arg_itr == m_args.end())
            throw std::runtime_error("unknown argument " + std::string(arg));

        return *arg_itr;
    }

    const ArgSpec& arg_spec_of(char arg) const
    {
        auto arg_itr = std::find_if(
            m_args.begin() + m_num_positional_args, m_args.end(),
            [&](const ArgSpec& my_arg) {
                return my_arg.as_short == arg;
            }
        );

        if (arg_itr == m_args.end())
            throw std::runtime_error(std::string("unknown argument ") + arg);

        return *arg_itr;
    }

    bool is_arg(const char* arg)
    {
        size_t length = strlen(arg);

        switch (length)
        {
            case 0:
            case 1:
                return false;
            case 2:
                return arg[0] == '-';
            default:
                return arg[0] == '-' && arg[1] == '-';
        }
    }

    std::string extract_full_arg(const char* arg)
    {
        size_t length = strlen(arg);

        switch (length)
        {
            case 0:
            case 1:
                return {};
            case 2:
                if (arg[0] != '-')
                    return {};
                return arg_spec_of(arg[1]).as_full;
            default:
                if (arg[0] != '-' || arg[1] != '-')
                    return {};
                return arg_spec_of(arg + 2).as_full;
        }
    }

private:
    help_callback m_help_callback;
    std::vector<ArgSpec> m_args;
    size_t m_num_positional_args;
    std::unordered_map<std::string, std::vector<std::string>> m_parsed_args;
};
