#include <uacpi/internal/opregion.h>
#include <uacpi/namespace.h>
#include <uacpi/kernel_api.h>

struct memory_region_ctx {
    uacpi_phys_addr phys;
    uacpi_u8 *virt;
    uacpi_size size;
};

static uacpi_status memory_region_attach(uacpi_region_attach_data *data)
{
    struct memory_region_ctx *ctx;
    uacpi_operation_region *op_region;
    uacpi_status ret = UACPI_STATUS_OK;

    ctx = uacpi_kernel_alloc(sizeof(*ctx));
    if (ctx == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    op_region = uacpi_namespace_node_get_object(data->region_node)->op_region;
    ctx->size = op_region->length;

    // FIXME: this really shouldn't try to map everything at once
    ctx->phys = op_region->offset;
    ctx->virt = uacpi_kernel_map(ctx->phys, ctx->size);

    if (uacpi_unlikely(ctx->virt == UACPI_NULL)) {
        ret = UACPI_STATUS_MAPPING_FAILED;
        uacpi_trace_region_error(data->region_node, "unable to map", ret);
        uacpi_kernel_free(ctx);
        return ret;
    }

    data->out_region_context = ctx;
    return ret;
}

static uacpi_status memory_region_detach(uacpi_region_detach_data *data)
{
    struct memory_region_ctx *ctx = data->region_context;

    uacpi_kernel_unmap(ctx->virt, ctx->size);
    uacpi_kernel_free(ctx);
    return UACPI_STATUS_OK;
}

struct io_region_ctx {
    uacpi_io_addr base;
    uacpi_handle handle;
};

static uacpi_status io_region_attach(uacpi_region_attach_data *data)
{
    struct io_region_ctx *ctx;
    uacpi_operation_region *op_region;
    uacpi_status ret;

    ctx = uacpi_kernel_alloc(sizeof(*ctx));
    if (ctx == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    op_region = uacpi_namespace_node_get_object(data->region_node)->op_region;
    ctx->base = op_region->offset;

    ret = uacpi_kernel_io_map(ctx->base, op_region->length, &ctx->handle);
    if (uacpi_unlikely_error(ret)) {
        uacpi_trace_region_error(
            data->region_node, "unable to map an IO", ret
        );
        uacpi_kernel_free(ctx);
        return ret;
    }

    data->out_region_context = ctx;
    return ret;
}

static uacpi_status io_region_detach(uacpi_region_detach_data *data)
{
    struct io_region_ctx *ctx = data->region_context;

    uacpi_kernel_io_unmap(ctx->handle);
    uacpi_kernel_free(ctx);
    return UACPI_STATUS_OK;
}

static uacpi_status memory_read(void *ptr, uacpi_u8 width, uacpi_u64 *out)
{
    switch (width) {
    case 1:
        *out = *(volatile uacpi_u8*)ptr;
        break;
    case 2:
        *out = *(volatile uacpi_u16*)ptr;
        break;
    case 4:
        *out = *(volatile uacpi_u32*)ptr;
        break;
    case 8:
        *out = *(volatile uacpi_u64*)ptr;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status memory_write(void *ptr, uacpi_u8 width, uacpi_u64 in)
{
    switch (width) {
    case 1:
        *(volatile uacpi_u8*)ptr = in;
        break;
    case 2:
        *(volatile uacpi_u16*)ptr = in;
        break;
    case 4:
        *(volatile uacpi_u32*)ptr = in;
        break;
    case 8:
        *(volatile uacpi_u64*)ptr = in;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status memory_region_do_rw(
    uacpi_region_op op, uacpi_region_rw_data *data
)
{
    struct memory_region_ctx *ctx = data->region_context;
    uacpi_u8 *ptr;

    ptr = ctx->virt + (data->address - ctx->phys);

    return op == UACPI_REGION_OP_READ ?
        memory_read(ptr, data->byte_width, &data->value) :
        memory_write(ptr, data->byte_width, data->value);
}

static uacpi_status handle_memory_region(uacpi_region_op op, uacpi_handle op_data)
{
    switch (op) {
    case UACPI_REGION_OP_ATTACH:
        return memory_region_attach(op_data);
    case UACPI_REGION_OP_DETACH:
        return memory_region_detach(op_data);
    case UACPI_REGION_OP_READ:
    case UACPI_REGION_OP_WRITE:
        return memory_region_do_rw(op, op_data);
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
}

static uacpi_status io_region_do_rw(
    uacpi_region_op op, uacpi_region_rw_data *data
)
{
    struct io_region_ctx *ctx = data->region_context;
    uacpi_u8 width;
    uacpi_size offset;

    offset = data->offset - ctx->base;
    width = data->byte_width;

    return op == UACPI_REGION_OP_READ ?
        uacpi_kernel_io_read(ctx->handle, offset, width, &data->value) :
        uacpi_kernel_io_write(ctx->handle, offset, width, data->value);
}

static uacpi_status handle_io_region(uacpi_region_op op, uacpi_handle op_data)
{
    switch (op) {
    case UACPI_REGION_OP_ATTACH:
        return io_region_attach(op_data);
    case UACPI_REGION_OP_DETACH:
        return io_region_detach(op_data);
    case UACPI_REGION_OP_READ:
    case UACPI_REGION_OP_WRITE:
        return io_region_do_rw(op, op_data);
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
}

void uacpi_install_default_address_space_handlers(void)
{
    uacpi_namespace_node *root;

    root = uacpi_namespace_root();

    uacpi_install_address_space_handler(
        root, UACPI_ADDRESS_SPACE_SYSTEM_MEMORY,
        handle_memory_region, UACPI_NULL
    );

    uacpi_install_address_space_handler(
        root, UACPI_ADDRESS_SPACE_SYSTEM_IO,
        handle_io_region, UACPI_NULL
    );
}
