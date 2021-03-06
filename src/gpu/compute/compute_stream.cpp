/*******************************************************************************
 * Copyright 2020 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "gpu/compute/compute_stream.hpp"
#include "gpu/compute/compute_engine.hpp"
#include "gpu/gpu_primitive.hpp"

namespace dnnl {
namespace impl {
namespace gpu {
namespace compute {
status_t compute_stream_t::zero_pad(const memory_t *memory) {
    memory_desc_wrapper mdw(memory->md());

    if (mdw.format_kind() != format_kind::blocked) return status::unimplemented;

    if (mdw.nelems(false) == mdw.nelems(true)) return status::success;

    // Kernel only compiled to support data types of length 1, 2, or 4 currently
    if (!utils::one_of(mdw.data_type_size(), 1u, 2u, 4u))
        return status::unimplemented;

    const blocking_desc_t blocking_desc = mdw.blocking_desc();

    const int max_step_nelems = ZERO_PAD_MAX_STEP_SIZE;
    size_t step_nelems = 1;
    for (int i = 0; i < blocking_desc.inner_nblks; i++) {
        step_nelems *= blocking_desc.inner_blks[i];
    }

    assert(step_nelems <= max_step_nelems);
    if (step_nelems > max_step_nelems) return stream_t::zero_pad(memory);

    engine_t *engine = this->engine();

    primitive_t *zero_pad_primitive;
    const resource_mapper_t *mapper;
    CHECK(utils::downcast<compute_engine_t *>(engine)->get_zero_pad_primitive(
            zero_pad_primitive, mapper));

    exec_args_t zero_pad_args;
    memory_arg_t arg = {const_cast<memory_t *>(memory), true};
    zero_pad_args[DNNL_ARG_SRC] = arg;
    exec_ctx_t zero_pad_ctx(this, std::move(zero_pad_args));
    zero_pad_ctx.set_resource_mapper(mapper);
    if (get_verbose()) {
        const int str_size = 64;
        char md_fmt[str_size];
        char md_dim[str_size];
        dnnl_md2fmt_str(md_fmt, str_size, memory->md());
        dnnl_md2dim_str(md_dim, str_size, memory->md());

        this->wait();
        double ms = get_msec();
        CHECK(zero_pad_primitive->execute(zero_pad_ctx));
        status_t status = this->wait();
        ms = get_msec() - ms;

        printf("dnnl_verbose,exec,%s,%s,%s,%s,%g\n", "gpu,zero_pad",
                zero_pad_primitive->pd()->name(), md_fmt, md_dim, ms);

        return status;
    } else {
        CHECK(zero_pad_primitive->execute(zero_pad_ctx));
        return this->wait();
    }
};
} // namespace compute
} // namespace gpu
} // namespace impl
} // namespace dnnl
