/*******************************************************************************
 * Copyright 2021 FUJITSU LIMITED
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
#define UNROLL 10

#include <stdio.h>
#include <time.h>
#include <vector>

#include <xbyak_aarch64/xbyak_aarch64.h>
using namespace Xbyak_aarch64;
class Generator : public CodeGenerator {
    bool fusion_;
    PReg p_all = p7;
    const std::vector<ZReg> z_aux
            = {/*z21, z22,*/ z23, z24, z25, z26, z27, z28, z29, z30, z31};
    const std::vector<WReg> w_aux = {w23, w24, w25, w26, w27, w21, w22};
    const std::vector<XReg> x_aux = {x23, x24, x25, x26, x27, x21, x22};
    const ZRegS exp_log2ef = z_aux[0].s;
    const ZRegS exp_ln_flt_max_f = z_aux[1].s;
    const ZRegS exp_ln_flt_min_f = z_aux[2].s;
    const ZRegS exp_not_mask17 = z_aux[3].s;
    const ZRegS exp_coeff1 = z_aux[4].s;
    const ZRegS exp_coeff2 = z_aux[5].s;
    const ZRegS exp_one = z_aux[6].s;
    const XReg exp_num = x0;
    const XReg exp_dst = x1;
    const XReg exp_src = x2;

    uint64_t sveLen_ = 0;

    /* For exp */
    const uint32_t coef_exp_log2ef = 0x3fb8aa3b;
    const uint32_t coef_exp_ln_flt_max_f = 0x42b17218;
    const uint32_t coef_exp_ln_flt_min_f = 0xc2aeac50;
    const uint32_t coef_exp_coeff1 = 0x3f31721c;
    const uint32_t coef_exp_coeff2 = 0x3e772df2;
    const uint32_t coef_exp_not_mask17 = ~((1u << 17) - 1);

private:
    const XReg abi_param1 = x0;
    const XReg abi_param2 = x1;
    const XReg abi_param3 = x2;
    const XReg abi_param4 = x3;
    const XReg abi_param5 = x4;
    const XReg abi_param6 = x5;
    const XReg abi_param7 = x6;
    const XReg abi_param8 = x7;
    const XReg abi_not_param1 = x15;

    const std::vector<XReg> abi_save_gpr_regs
            = {x19, x20, x21, x22, x23, x24, x25, x26, x27, x28};
    const size_t xreg_len = 8;
    const size_t vreg_len_preserve = 8; // Only bottom 8byte must be preserved.
    const size_t vreg_to_preserve = 8; // VREG8 - VREG15

    //    const size_t num_abi_save_gpr_regs
    //            = sizeof(abi_save_gpr_regs) / sizeof(abi_save_gpr_regs[0]);
    const size_t num_abi_save_gpr_regs = abi_save_gpr_regs.size();

    const size_t preserved_stack_size = xreg_len * (2 + num_abi_save_gpr_regs)
            + vreg_len_preserve * vreg_to_preserve;

    const size_t size_of_abi_save_regs = num_abi_save_gpr_regs * x0.getBit() / 8
            + vreg_to_preserve * vreg_len_preserve;

public:
    void preamble() {
        stp(x29, x30, pre_ptr(sp, -16));
        /* x29 is a frame pointer. */
        mov(x29, sp);
        sub(sp, sp, static_cast<int64_t>(preserved_stack_size) - 16);

        /* x9 can be used as a temporal register. */
        mov(x9, sp);

        if (vreg_to_preserve) {
            st4((v8.d - v11.d)[0], post_ptr(x9, vreg_len_preserve * 4));
            st4((v12.d - v15.d)[0], post_ptr(x9, vreg_len_preserve * 4));
        }
        for (size_t i = 0; i < num_abi_save_gpr_regs; i += 2) {
            stp(Xbyak_aarch64::XReg(abi_save_gpr_regs[i]),
                    Xbyak_aarch64::XReg(abi_save_gpr_regs[i + 1]),
                    post_ptr(x9, xreg_len * 2));
        }

        if (sveLen_) { /* SVE is available. */
            ptrue(p_all.b);
        }
    }

    void postamble() {
        mov(x9, sp);

        if (vreg_to_preserve) {
            ld4((v8.d - v11.d)[0], post_ptr(x9, vreg_len_preserve * 4));
            ld4((v12.d - v15.d)[0], post_ptr(x9, vreg_len_preserve * 4));
        }

        for (size_t i = 0; i < num_abi_save_gpr_regs; i += 2) {
            ldp(Xbyak_aarch64::XReg(abi_save_gpr_regs[i]),
                    Xbyak_aarch64::XReg(abi_save_gpr_regs[i + 1]),
                    post_ptr(x9, xreg_len * 2));
        }

        add(sp, sp, static_cast<int64_t>(preserved_stack_size) - 16);
        ldp(x29, x30, Xbyak_aarch64::post_ptr(sp, 16));
        ret();
    }

    void gen_exp_body(const ZRegS &t0) {
        using f = void (CodeGenerator::*)(
                const ZRegS &, const _PReg &, const ZRegS &);
        const std::vector<ZRegS> t1
                = {ZRegS(10), ZRegS(11), ZRegS(12), ZRegS(13), ZRegS(14),
                        ZRegS(15), ZRegS(16), ZRegS(17), ZRegS(18), ZRegS(19)};
        const std::vector<ZRegS> t2
                = {ZRegS(20), ZRegS(21), ZRegS(22), ZRegS(23), ZRegS(24),
                        ZRegS(25), ZRegS(26), ZRegS(27), ZRegS(28), ZRegS(29)};

        auto loop_mn
                = [this](size_t start, size_t end, f &mn, PReg p, ZRegS src) {
                      for (size_t i = start; i < end; i++)
                          (this->*mn)(ZRegS(i), p, src);
                  };

        f mn_fmin = &CodeGenerator::fmin;
        f mn_fmax = &CodeGenerator::fmax;

        loop_mn(0, 10, mn_fmin, p_all, exp_ln_flt_max_f);
        loop_mn(0, 10, mn_fmax, p_all, exp_ln_flt_min_f);
        for (size_t i = 0; i < UNROLL; i++)
            fmul(ZRegS(i), ZRegS(i), exp_log2ef);
        //        for (size_t i = 0; i < UNROLL; i++)
        //            movprfx(t1[i], p_all, ZRegS(i));
        for (size_t i = 0; i < UNROLL; i++)
            frintm(t1[i], p_all, ZRegS(i));
        for (size_t i = 0; i < UNROLL; i++)
            fcvtzs(t2[i], p_all, t1[i]);
        for (size_t i = 0; i < UNROLL; i++)
            fsub(t1[i], ZRegS(i), t1[i]);
        for (size_t i = 0; i < UNROLL; i++)
            fadd(ZRegS(i), t1[i], exp_one);
        for (size_t i = 0; i < UNROLL; i++)
            lsr(t1[i], ZRegS(i), 17);
        for (size_t i = 0; i < UNROLL; i++)
            fexpa(t1[i], t1[i]);
        for (size_t i = 0; i < UNROLL; i++)
            fscale(t1[i], p_all, t2[i]);
        for (size_t i = 0; i < UNROLL; i++)
            and_(ZRegD(t2[i].getIdx()), ZRegD(ZRegS(i).getIdx()),
                    ZRegD(exp_not_mask17.getIdx()));
        for (size_t i = 0; i < UNROLL; i++)
            fsub(t2[i], ZRegS(i), t2[i]);
        //        for (size_t i = 0; i < UNROLL; i++)
        //            movprfx(ZRegS(i), p_all, exp_coeff2);
        for (size_t i = 0; i < UNROLL; i++)
            fmad(ZRegS(i), p_all, t2[i], exp_coeff1);
        for (size_t i = 0; i < UNROLL; i++)
            fmad(ZRegS(i), p_all, t2[i], exp_one);
        for (size_t i = 0; i < UNROLL; i++)
            fmul(ZRegS(i), t1[i], ZRegS(i));
    }

    void loadCoefExp() {
        // setup constant
        mov_imm(w_aux[0], coef_exp_log2ef);
        mov_imm(w_aux[1], coef_exp_ln_flt_max_f);
        mov_imm(w_aux[2], coef_exp_ln_flt_min_f);
        mov_imm(w_aux[3], coef_exp_not_mask17);
        mov_imm(w_aux[4], coef_exp_coeff1);
        mov_imm(w_aux[5], coef_exp_coeff2);
        dup(exp_log2ef, w_aux[0]);
        dup(exp_ln_flt_max_f, w_aux[1]);
        dup(exp_ln_flt_min_f, w_aux[2]);
        dup(exp_not_mask17, w_aux[3]);
        dup(exp_coeff1, w_aux[4]);
        dup(exp_coeff2, w_aux[5]);
        fdup(exp_one, 1.f);
    }

    void loadInput() {
        for (size_t i = 0; i < UNROLL; i++)
            ldr(ZReg(i), ptr(x2, i, MUL_VL));
    }
    void storeOutput() {
        for (size_t i = 0; i < UNROLL; i++)
            str(ZReg(i), ptr(x1, i, MUL_VL));
    }

    Generator(bool fusion = false) : fusion_(fusion) {
        preamble();
        loadCoefExp();
        Label l_begin = L();
        loadInput();

        gen_exp_body(ZRegS(0));
        if (fusion) {
            gen_exp_body(ZRegS(0));
            gen_exp_body(ZRegS(0));
        }

        storeOutput();

        subs(x0, x0, 1);
        b(GT, l_begin);
        postamble();
        ret();

        ready();
    }
};

void genRandomInput(size_t n, float *a) {
#pragma omp parallel for simd
    for (size_t i = 0; i < n; i++) {
        a[i] = 3.f;
    }
}

int main(int argc, char *argv[]) {
    size_t ite;
    float sum = 0.f;
    clock_t start;
    clock_t end;
    double time;
    bool fusion = atoi(argv[1]) > 0 ? true : false;
    ite = atoi(argv[2]);

    Generator gen(fusion);
    gen.ready();

    float *a = new float[UNROLL * 16 * ite];
    if (a == NULL) fprintf(stderr, "Can not allocate memory");

    start = clock();
    genRandomInput(UNROLL * 16 * ite, a);
    end = clock();
    time = static_cast<double>(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("time %lf[ms]\n", time);
    auto f = gen.getCode<int (*)(size_t, float *, float *)>();

    start = clock();
    f(ite, a, a);
    if (!fusion) f(ite, a, a);
    if (!fusion) f(ite, a, a);
    end = clock();

    time = static_cast<double>(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("time %lf[ms]\n", time);

    for (size_t i = 0; i < UNROLL * 16 * ite; i++)
        sum += a[i];
    delete (a);

    bool dump = true;
    if (dump) {
        FILE *fp = fopen("hoge.bin", "wb");
        fwrite(reinterpret_cast<void *>(f), sizeof(uint32_t), gen.getSize(),
                fp);
        fclose(fp);
    }

    return sum;
}
