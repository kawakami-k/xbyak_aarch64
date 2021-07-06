#*******************************************************************************
# Copyright 2019-2021 FUJITSU LIMITED
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# *******************************************************************************/
SRC=make_nm_branch.cpp make_nm_fp.cpp make_nm_load_store.cpp make_nm_simd0.cpp make_nm_simd1.cpp make_nm_simd2.cpp make_nm_simd3.cpp make_nm_simd4.cpp make_nm_simd5.cpp make_nm_simd6.cpp make_nm_simd_fp_load_store.cpp make_nm_sve.cpp make_nm_sve_addr0.cpp make_nm_sve_addr1.cpp make_nm_sve_addr2.cpp make_nm_sve_addr3.cpp
#SRC=make_nm_branch.cpp make_nm_fp.cpp make_nm_load_store.cpp make_nm_simd_fp_load_store.cpp make_nm_sve.cpp make_nm_sve_addr.cpp
#SRC=make_nm_branch.cpp
EXE=$(SRC:.cpp=.exe)
TMP_ASM=$(SRC:.cpp=.tmp_asm)
ASM=$(SRC:.cpp=.asm)
LST=$(SRC:.cpp=.lst)
OK=$(SRC:.cpp=.ok)
JIT_HPP=$(SRC:.cpp=.jit.hpp)
FRAME_EXE=$(SRC:.cpp=.frame_exe)
TXT=$(SRC:.cpp=.txt)
RESULT=$(SRC:.cpp=.result)

CXX_FLAGS1=-std=c++11 -fomit-frame-pointer -Wall -fno-operator-names -I../xbyak_aarch64 -I./ -Wall -Wextra -Wformat=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith -Wno-ignored-qualifiers -fsanitize=address -lasan
CXX_FLAGS2=-std=c++11 -Wall -I../xbyak_aarch64 -DXBYAK_TEST -DXBYAK_USE_MMAP_ALLOCATOR -fsanitize=address -lasan
AARCH64_TYPE=armv8.4-a
AWK=awk
SED=sed

%.exe: %.cpp
	$(CXX) $(CXX_FLAGS1) -o $@ $<

%.tmp_asm: %.exe
	./$< > $@

%.asm: %.tmp_asm
	@cat $< \
	| $(SED) -e "s/BReg(\([0-9]\+\))/b\1/g" \
	| $(SED) -e "s/HReg(\([0-9]\+\))/h\1/g" \
	| $(SED) -e "s/SReg(\([0-9]\+\))/s\1/g" \
	| $(SED) -e "s/DReg(\([0-9]\+\))/d\1/g" \
	| $(SED) -e "s/QReg(\([0-9]\+\))/q\1/g" \
	| $(SED) -e "s/WReg(\([0-9]\+\))/w\1/g" \
	| $(SED) -e "s/XReg(\([0-9]\+\))/x\1/g" \
	| $(SED) -e "s/VReg(\([0-9]\+\))/v\1/g" \
	| $(SED) -e "s/ZReg(\([0-9]\+\))/z\1/g" \
	> $@

%.lst: %.asm
	$(AS) -march=$(AARCH64_TYPE) -mcpu=all -acdnl=$@ $<

%.ok: %.lst
	@cat $< | grep -v -e "^\*\*\*\*" \
	| $(AWK) '{print substr($$3, 7, 2) substr($$3, 5, 2) substr($$3, 3, 2) substr($$3, 1, 2); }' \
	| grep -v -e "^$$" > $@

%.jit.hpp: %.exe
	./$< jit > $@

%.frame_exe: %.jit.hpp nm_frame.cpp
	@cat nm_frame.cpp | $(SED) -e "s/TEST_PTN/$</" > nm_frame.$(basename $<).cpp
	$(CXX) $(CXX_FLAGS2) -o $@ nm_frame.$(basename $<).cpp ../lib/libxbyak_aarch64.a

%.txt: %.frame_exe
	./$< > $@

%.result: %.txt %.ok
	diff -w $^
	@diff -w $^ > $@

all: $(RESULT)

clean:
	rm -rf *.exe *.tmp_asm *.asm *.lst *.ok *.jit.hpp *.frame_exe *.txt *.result nm_frame.*.cpp


.DEFAULT_GOAL := all
.PHONY: clean all_result
.PRECIOUS: $(EXE) $(TMP_ASM) $(LST) $(OK) $(JIT_HPP) $(FRAME_EXE) $(TXT)
