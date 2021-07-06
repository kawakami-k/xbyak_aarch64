#include <xbyak_aarch64/xbyak_aarch64.h>
using namespace Xbyak_aarch64;
class Generator : public CodeGenerator {
public:
  Generator() {
    stp(xzr, xzr, pre_ptr(sp, -16));
    ldp(xzr, xzr, post_ptr(sp, 16));
  }
};

int main() {
  Generator gen;
  gen.ready();
  return 0;
}
