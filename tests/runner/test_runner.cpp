#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <cinttypes>

#include "helpers.h"
#include <uacpi/notify.h>
#include <uacpi/utilities.h>

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

static void enumerate_namespace()
{
    uacpi_namespace_for_each_node_depth_first(
        uacpi_namespace_root(), [] (void*, uacpi_namespace_node *node) {
            uacpi_namespace_node_info *info;

            auto depth = uacpi_namespace_node_depth(node);

            auto nested_printf = [depth](const char *fmt, ...) {
                va_list va;
                size_t padding = depth * 4;

                while (padding-- > 0)
                    std::printf(" ");

                va_start(va, fmt);
                std::vprintf(fmt, va);
                va_end(va);
            };

            auto ret = uacpi_get_namespace_node_info(node, &info);
            if (uacpi_unlikely_error(ret)) {
                fprintf(
                    stderr, "unable to get node %.4s info: %s\n",
                    uacpi_namespace_node_name(node).text,
                    uacpi_status_to_string(ret)
                );
                std::exit(1);
            }

            auto *path = uacpi_namespace_node_generate_absolute_path(node);
            nested_printf(
                "%s [%s]", path, uacpi_object_type_to_string(info->type)
            );
            uacpi_free_absolute_path(path);

            if (info->type == UACPI_OBJECT_METHOD)
                std::printf(" (%d args)", info->num_params);

            auto has_info = info->has_adr || info->has_hid ||
                            info->has_cid || info->has_uid ||
                            info->has_cls || info->has_sxd ||
                            info->has_sxw;

            if (has_info)
                std::printf(" {\n");

            if (info->has_adr)
                nested_printf("  _ADR: %016" PRIX64 "\n", info->adr);

            if (info->has_hid)
                nested_printf("  _HID: %s\n", info->hid.value);
            if (info->has_cid) {
                nested_printf("  _CID: ");
                for (size_t i = 0; i < info->cid.num_ids; ++i)
                    std::printf("%s ", info->cid.ids[i].value);

                std::printf("\n");
            }
            if (info->has_uid)
                nested_printf("  _UID: %s\n", info->uid.value);
            if (info->has_cls)
                nested_printf("  _CLS: %s\n", info->cls.value);

            if (info->has_sxd) {
                nested_printf(
                    "  _SxD: S1->D%d S2->D%d S3->D%d S4->D%d\n",
                    info->sxd[0], info->sxd[1], info->sxd[2], info->sxd[3]
                );
            }
            if (info->has_sxw) {
                nested_printf(
                    "  _SxW: S0->D%d S1->D%d S2->D%d S3->D%d S4->D%d\n",
                    info->sxw[0], info->sxw[1], info->sxw[2], info->sxw[3],
                    info->sxw[4]
                );
            }

            if (has_info)
                nested_printf("}\n");
            else
                std::printf("\n");

            uacpi_free_namespace_node_info(info);
            return UACPI_NS_ITERATION_DECISION_CONTINUE;
        }, nullptr
    );
}

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

enum class run_mode {
    EMULATION,
    EMULATION_WITH_NAMESPACE_DUMP,
    TEST,
    RESOURCE_TESTS,
};

static void run_test(
    run_mode mode, std::string_view dsdt_path, uacpi_object_type expected_type,
    std::string_view expected_value
)
{
    acpi_rsdp rsdp {};
    full_xsdt xsdt {};

    build_xsdt_from_file(xsdt, rsdp, dsdt_path);
    auto dsdt_delete = ScopeGuard(
        [&xsdt] {
            delete[] reinterpret_cast<uint8_t*>(
                static_cast<uintptr_t>(xsdt.fadt->x_dsdt)
            );

            delete xsdt.fadt;
        }
    );

    uacpi_log_level level = UACPI_LOG_TRACE;

    // Don't spam the log with traces if enumeration is enabled
    if (mode == run_mode::EMULATION_WITH_NAMESPACE_DUMP)
        level = UACPI_LOG_INFO;

    uacpi_init_params params = {
        reinterpret_cast<uacpi_phys_addr>(&rsdp),
        { level, 0 },
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

    g_expect_virtual_addresses = false;

    st = uacpi_install_notify_handler(
        uacpi_namespace_root(), handle_notify, nullptr
    );
    ensure_ok_status(st);

    st = uacpi_namespace_load();
    ensure_ok_status(st);

    st = uacpi_namespace_initialize();
    ensure_ok_status(st);

    switch (mode) {
    case run_mode::EMULATION_WITH_NAMESPACE_DUMP:
        enumerate_namespace();
        [[fallthrough]];
    case run_mode::EMULATION:
        // We're done with emulation mode
        return;
    default:
        break;
    }

    uacpi_object* ret = UACPI_NULL;
    auto guard = ScopeGuard(
        [&ret] { uacpi_object_unref(ret); }
    );

    st = uacpi_eval(UACPI_NULL, "\\MAIN", UACPI_NULL, &ret);

    ensure_ok_status(st);
    validate_ret_against_expected(*ret, expected_type, expected_value);
}

run_mode get_run_mode(int argc, char **argv)
{
    switch (argc) {
    case 2: {
        auto arg_view = std::string_view(argv[1]);

        if (arg_view == "--test-resources")
            return run_mode::RESOURCE_TESTS;

        if (arg_view == "--help" || arg_view == "?" || arg_view == "-h")
            break;

        return run_mode::EMULATION;
    }
    case 3:
        if (std::string_view(argv[2]) != "--enumerate-namespace")
            break;

        return run_mode::EMULATION_WITH_NAMESPACE_DUMP;
    case 4:
        return run_mode::TEST;
    default:
        break;
    }

    std::cout << "Usage:"
        << "\n[EMULATION] " << argv[0] << " <dsdt_path> {--enumerate-namespace}"
        << "\n[TEST MODE] " << argv[0]
        << " <dsdt_path> <expected_type> <expected_value>"
        << "\n[RESOURCE TESTS] " << argv[0] << " --test-resources\n";
    std::exit(1);
}

int main(int argc, char** argv)
{
    auto mode = get_run_mode(argc, argv);

    try {
        if (mode == run_mode::RESOURCE_TESTS) {
            run_resource_tests();
            return 0;
        }

        std::string_view expected_value;
        uacpi_object_type expected_type = UACPI_OBJECT_UNINITIALIZED;

        if (mode == run_mode::TEST) {
            expected_type = string_to_object_type(argv[2]);
            expected_value = argv[3];
        }

        run_test(mode, argv[1], expected_type, expected_value);
    } catch (const std::exception& ex) {
        std::cerr << "unexpected error: " << ex.what() << std::endl;
        return 1;
    }
}
