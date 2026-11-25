#pragma once
namespace stp {
constexpr bool AllowCleoSignature(unsigned type, int signature) {
    return type == 0 && (signature == 40 || signature == 41);
}
}
