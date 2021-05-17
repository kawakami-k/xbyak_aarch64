/*******************************************************************************
 * Copyright 2019-2020 FUJITSU LIMITED
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
#include <cmath>
#include <random>

#include <xbyak_aarch64/xbyak_aarch64.h>
using namespace Xbyak_aarch64;
class Generator : public CodeGenerator {
public:
    Generator() {
        add(w0, w1, w0);
        ret();
    }
};
int main() {
    Generator gen;
    gen.ready();
    auto f = gen.getCode<int (*)(int, int)>();
    int a = 3;
    int b = 4;
    printf("%d + %d = %d\n", a, b, f(a, b));

    float c[16];
    float d[16];
    float retVal = 0.f;

    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_real_distribution<> dist1(-1.0, 1.0);

    for (size_t i = 0; i < 16; i++) {
        c[i] = dist1(engine);
    }

    for (size_t i = 0; i < 16; i++) {
        d[i] = std::cosh(c[i]);
    }

    for (size_t i = 0; i < 16; i++) {
        retVal += d[i];
    }

    printf("retVal=%f\n", retVal);

    return 0;
}
