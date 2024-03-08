#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>

#include "helpers.h"
#include <uacpi/notify.h>

void run_resource_tests();

static uacpi_object_type string_to_object_type(std::string_view str)
{
    if (str == "int")
        return UACPI_OBJECT_INTEGER;
    if (str == "str")
        return UACPI_OBJECT_STRING;

    throw std::runtime_error(
        std::string("Unsupported type for validation: ") + str.data()
    );
}

static void validate_ret_against_expected(
    uacpi_object& obj, uacpi_object_type expected_type,
    std::string_view expected_val
)
{
    auto ret_is_wrong = [](std::string_view expected, std::string_view actual)
    {
        std::string err;
        err += "returned value '";
        err += actual.data();
        err += "' doesn't match expected '";
        err += expected.data();
        err += "'";

        throw std::runtime_error(err);
    };


    if (obj.type != expected_type) {
        std::string err;
        err += "returned type '";
        err += uacpi_object_type_to_string((uacpi_object_type)obj.type);
        err += "' doesn't match expected '";
        err += uacpi_object_type_to_string(expected_type);
        err += "'";

        throw std::runtime_error(err);
    }

    switch (obj.type) {
    case UACPI_OBJECT_INTEGER: {
        auto expected_int = std::stoull(expected_val.data(), nullptr, 0);
        auto& actual_int = obj.integer;

        if (expected_int != actual_int)
            ret_is_wrong(expected_val, std::to_string(actual_int));
    } break;
    case UACPI_OBJECT_STRING: {
        auto actual_str = std::string_view(obj.buffer->text,
                                           obj.buffer->size - 1);

        if (expected_val != actual_str)
            ret_is_wrong(expected_val, actual_str);
    } break;
    default:
        std::abort();
    }
}

template <typename ExprT>
class ScopeGuard
{
public:
    ScopeGuard(ExprT expr)
        : callback(std::move(expr)) {}

    ~ScopeGuard() { callback(); }

private:
    ExprT callback;
};

static uacpi_status handle_notify(
    uacpi_handle, uacpi_namespace_node *node, uacpi_u64 value
)
{
    auto *path = uacpi_namespace_node_generate_absolute_path(node);
    auto guard = ScopeGuard([path] { std::free((void*)path); });

    std::cout << "Received a notification from " << path << " "
              << std::hex << value << std::endl;

    return UACPI_STATUS_OK;
}

static void run_test(
    std::string_view dsdt_path, uacpi_object_type expected_type,
    std::string_view expected_value
)
{
    acpi_rsdp rsdp {};
    full_xsdt xsdt {};

    build_xsdt_from_file(xsdt, rsdp, dsdt_path);

    uacpi_init_params params = {
        reinterpret_cast<uacpi_phys_addr>(&rsdp),
        { UACPI_LOG_TRACE, 0 },
        UACPI_TRUE, // don't attempt to enter ACPI mode in userspace
    };

    auto ensure_ok_status = [] (uacpi_status st) {
        if (st == UACPI_STATUS_OK)
            return;

        auto msg = uacpi_status_to_string(st);
        throw std::runtime_error(std::string("uACPI error: ") + msg);
    };

    uacpi_status st = uacpi_initialize(&params);
    ensure_ok_status(st);

    st = uacpi_install_notify_handler(
        uacpi_namespace_root(), handle_notify, nullptr
    );
    ensure_ok_status(st);

    st = uacpi_namespace_load();
    ensure_ok_status(st);

    st = uacpi_namespace_initialize();
    ensure_ok_status(st);

    if (expected_type == UACPI_OBJECT_UNINITIALIZED)
        // We're done with emulation mode
        return;

    uacpi_object* ret = UACPI_NULL;
    auto guard = ScopeGuard(
        [&ret] { uacpi_object_unref(ret); }
    );

    st = uacpi_eval(UACPI_NULL, "\\MAIN", UACPI_NULL, &ret);

    ensure_ok_status(st);
    validate_ret_against_expected(*ret, expected_type, expected_value);
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 4) {
        std::cout << "Usage:"
            << "\n[EMULATION] " << argv[0] << " <dsdt_path>"
            << "\n[TEST MODE] " << argv[0]
            << " <dsdt_path> <expected_type> <expected_value>"
            << "\n[RESOURCE TESTS] " << argv[0] << " --test-resources"
            << std::endl;
        return 1;
    }

    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test-resources") {
            run_resource_tests();
            return 0;
        }

        std::string_view expected_value;
        uacpi_object_type expected_type = UACPI_OBJECT_UNINITIALIZED;

        if (argc == 4) {
            expected_type = string_to_object_type(argv[2]);
            expected_value = argv[3];
        }

        run_test(argv[1], expected_type, expected_value);
    } catch (const std::exception& ex) {
        std::cerr << "unexpected error: " << ex.what() << std::endl;
        return 1;
    }
}
