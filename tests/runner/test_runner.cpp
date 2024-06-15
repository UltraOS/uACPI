#include <iostream>
#include <filesystem>
#include <string>
#include <cstring>
#include <string_view>
#include <cinttypes>

#include "helpers.h"
#include "argparser.h"
#include <uacpi/context.h>
#include <uacpi/notify.h>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include <uacpi/osi.h>
#include <uacpi/tables.h>

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

            if (info->flags)
                std::printf(" {\n");

            if (info->flags)
                nested_printf("  _ADR: %016" PRIX64 "\n", info->adr);

            if (info->flags & UACPI_NS_NODE_INFO_HAS_HID)
                nested_printf("  _HID: %s\n", info->hid.value);
            if (info->flags & UACPI_NS_NODE_INFO_HAS_CID) {
                nested_printf("  _CID: ");
                for (size_t i = 0; i < info->cid.num_ids; ++i)
                    std::printf("%s ", info->cid.ids[i].value);

                std::printf("\n");
            }
            if (info->flags & UACPI_NS_NODE_INFO_HAS_UID)
                nested_printf("  _UID: %s\n", info->uid.value);
            if (info->flags & UACPI_NS_NODE_INFO_HAS_CLS)
                nested_printf("  _CLS: %s\n", info->cls.value);

            if (info->flags & UACPI_NS_NODE_INFO_HAS_SXD) {
                nested_printf(
                    "  _SxD: S1->D%d S2->D%d S3->D%d S4->D%d\n",
                    info->sxd[0], info->sxd[1], info->sxd[2], info->sxd[3]
                );
            }
            if (info->flags & UACPI_NS_NODE_INFO_HAS_SXW) {
                nested_printf(
                    "  _SxW: S0->D%d S1->D%d S2->D%d S3->D%d S4->D%d\n",
                    info->sxw[0], info->sxw[1], info->sxw[2], info->sxw[3],
                    info->sxw[4]
                );
            }

            if (info->flags) {
                auto dump_resources = [=](auto cb, const char *name) {
                    uacpi_resources *res;

                    auto ret = cb(node, &res);
                    if (ret == UACPI_STATUS_OK) {
                        // TODO: dump resources here
                        nested_printf("  %s: <%u bytes>\n", name, res->length);
                        uacpi_free_resources(res);
                    } else if (ret != UACPI_STATUS_NOT_FOUND) {
                        nested_printf(
                            "  %s: unable to evaluate (%s)\n",
                            name, uacpi_status_to_string(ret)
                        );
                    }
                };

                if (info->type == UACPI_OBJECT_DEVICE) {
                    dump_resources(uacpi_get_current_resources, "_CRS");
                    dump_resources(uacpi_get_possible_resources, "_PRS");
                }

                nested_printf("}\n");
            } else {
                std::printf("\n");
            }

            uacpi_free_namespace_node_info(info);
            return UACPI_NS_ITERATION_DECISION_CONTINUE;
        }, nullptr
    );
}

/*
 * DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "OVERRIDE", 0xF0F0F0F0)
 * {
 *     Name (VAL, "TestRunner")
 * }
 */
uint8_t table_override[] = {
    0x53, 0x53, 0x44, 0x54, 0x35, 0x00, 0x00, 0x00,
    0x01, 0xa1, 0x75, 0x54, 0x45, 0x53, 0x54, 0x00,
    0x4f, 0x56, 0x45, 0x52, 0x52, 0x49, 0x44, 0x45,
    0xf0, 0xf0, 0xf0, 0xf0, 0x49, 0x4e, 0x54, 0x4c,
    0x25, 0x09, 0x20, 0x20, 0x08, 0x56, 0x41, 0x4c,
    0x5f, 0x0d, 0x54, 0x65, 0x73, 0x74, 0x52, 0x75,
    0x6e, 0x6e, 0x65, 0x72, 0x00
};

/*
 * DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "RUNRIDTB", 0xF0F0F0F0)
 * {
 *     Name (\_SI.TID, "uACPI")
 *     Printf("TestRunner ID SSDT loaded!")
 * }
 */
uint8_t runner_id_table[] = {
    0x53, 0x53, 0x44, 0x54, 0x55, 0x00, 0x00, 0x00,
    0x01, 0x45, 0x75, 0x54, 0x45, 0x53, 0x54, 0x00,
    0x52, 0x55, 0x4e, 0x52, 0x49, 0x44, 0x54, 0x42,
    0xf0, 0xf0, 0xf0, 0xf0, 0x49, 0x4e, 0x54, 0x4c,
    0x25, 0x09, 0x20, 0x20, 0x08, 0x5c, 0x2e, 0x5f,
    0x53, 0x49, 0x5f, 0x54, 0x49, 0x44, 0x5f, 0x0d,
    0x75, 0x41, 0x43, 0x50, 0x49, 0x00, 0x70, 0x0d,
    0x54, 0x65, 0x73, 0x74, 0x52, 0x75, 0x6e, 0x6e,
    0x65, 0x72, 0x20, 0x49, 0x44, 0x20, 0x53, 0x53,
    0x44, 0x54, 0x20, 0x6c, 0x6f, 0x61, 0x64, 0x65,
    0x64, 0x21, 0x00, 0x5b, 0x31
};

static uacpi_table_installation_disposition handle_table_install(
    struct acpi_sdt_hdr *hdr, uacpi_u64 *out_override
)
{
    if (strncmp(hdr->oem_table_id, "DENYTABL", sizeof(hdr->oem_table_id)) == 0)
        return UACPI_TABLE_INSTALLATION_DISPOSITON_DENY;

    if (strncmp(hdr->oem_table_id, "OVERTABL", sizeof(hdr->oem_table_id)) != 0)
        return UACPI_TABLE_INSTALLATION_DISPOSITON_ALLOW;

    *out_override = (uacpi_virt_addr)table_override;
    return UACPI_TABLE_INSTALLATION_DISPOSITON_VIRTUAL_OVERRIDE;
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

static void run_test(
    std::string_view dsdt_path, uacpi_object_type expected_type,
    std::string_view expected_value, bool dump_namespace
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
            delete reinterpret_cast<acpi_facs*>(
                static_cast<uintptr_t>(xsdt.fadt->x_firmware_ctrl)
            );
            delete xsdt.fadt;
        }
    );

    uacpi_log_level level = UACPI_LOG_TRACE;

    // Don't spam the log with traces if enumeration is enabled
    if (dump_namespace)
        level = UACPI_LOG_INFO;

    uacpi_init_params params = {
        reinterpret_cast<uacpi_phys_addr>(&rsdp),
        level,
        UACPI_FLAG_NO_ACPI_MODE, // don't attempt to enter ACPI mode in userspace
    };

    auto ensure_ok_status = [] (uacpi_status st) {
        if (st == UACPI_STATUS_OK)
            return;

        auto msg = uacpi_status_to_string(st);
        throw std::runtime_error(std::string("uACPI error: ") + msg);
    };

    uacpi_status st = uacpi_initialize(&params);
    ensure_ok_status(st);

    // We really don't need more than this
    uacpi_context_set_loop_timeout(3);

    g_expect_virtual_addresses = false;

    st = uacpi_install_notify_handler(
        uacpi_namespace_root(), handle_notify, nullptr
    );
    ensure_ok_status(st);

    st = uacpi_set_table_installation_handler(handle_table_install);
    ensure_ok_status(st);

    st = uacpi_install_interface("TestRunner", UACPI_INTERFACE_KIND_FEATURE);
    ensure_ok_status(st);

    st = uacpi_uninstall_interface("Windows 2006");
    ensure_ok_status(st);

    st = uacpi_uninstall_interface("Windows 2006");
    if (st != UACPI_STATUS_NOT_FOUND)
        throw std::runtime_error("couldn't uninstall interface");

    st = uacpi_enable_host_interface(UACPI_HOST_INTERFACE_3_0_THERMAL_MODEL);
    ensure_ok_status(st);

    st = uacpi_enable_host_interface(UACPI_HOST_INTERFACE_MODULE_DEVICE);
    ensure_ok_status(st);

    auto is_test_mode = expected_type != UACPI_OBJECT_UNINITIALIZED;
    if (is_test_mode) {
        st = uacpi_table_install(runner_id_table, UACPI_NULL);
        ensure_ok_status(st);
    }

    st = uacpi_namespace_load();
    ensure_ok_status(st);

    if (is_test_mode) {
        uacpi_object *runner_id;
        st = uacpi_eval_typed(UACPI_NULL, "\\_SI.TID", UACPI_NULL,
                              UACPI_OBJECT_STRING_BIT, &runner_id);
        ensure_ok_status(st);

        if (strcmp(runner_id->buffer->text, "uACPI") != 0)
            throw std::runtime_error("invalid test runner id");
        uacpi_object_unref(runner_id);
    }

    st = uacpi_namespace_initialize();
    ensure_ok_status(st);

    if (dump_namespace)
        enumerate_namespace();

    if (!is_test_mode)
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
    auto args = ArgParser {};
    args.add_positional(
            "dsdt-path-or-keyword",
            "path to the DSDT to run or \"resource-tests\" to run the resource "
            "tests and exit"
        )
        .add_list(
            "expect", 'r', "test mode, evaluate \\MAIN and expect "
            "<expected_type> <expected_value>"
        )
        .add_flag(
            "enumerate-namespace", 'd',
            "dump the entire namespace after loading it"
        )
        .add_help(
            "help", 'h', "Display this menu and exit",
            [&]() { std::cout << "uACPI test runner:\n" << args; }
        );

    try {
        args.parse(argc, argv);

        auto dsdt_path_or_keyword = args.get("dsdt-path-or-keyword");
        if (dsdt_path_or_keyword == "resource-tests") {
            run_resource_tests();
            return 0;
        }

        std::string_view expected_value;
        uacpi_object_type expected_type = UACPI_OBJECT_UNINITIALIZED;

        if (args.is_set('r')) {
            auto& expect = args.get_list('r');
            if (expect.size() != 2)
                throw std::runtime_error("bad --expect format");

            expected_type = string_to_object_type(expect[0]);
            expected_value = expect[1];
        }

        run_test(dsdt_path_or_keyword, expected_type, expected_value,
                 args.is_set('d'));
    } catch (const std::exception& ex) {
        std::cerr << "unexpected error: " << ex.what() << std::endl;
        return 1;
    }
}
