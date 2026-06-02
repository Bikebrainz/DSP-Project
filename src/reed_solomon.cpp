// Systematic Reed-Solomon over GF(2^8), primitive polynomial 0x11D, generator 2.
//
// The arithmetic follows the standard textbook construction (Berlekamp-Massey
// error-locator search, Chien search for roots, Forney algorithm for error
// magnitudes). Polynomials are stored most-significant-coefficient first, as in
// the common reference implementations.
#include "emcast/fec.hpp"

#include <algorithm>

namespace emcast {
namespace {

constexpr int kFieldChar = 255;  // 2^8 - 1

// Galois field GF(2^8) exponent/log tables, built once.
struct GFTables {
    std::uint8_t exp[512];
    std::uint8_t log[256];

    GFTables() {
        int x = 1;
        for (int i = 0; i < kFieldChar; ++i) {
            exp[i] = static_cast<std::uint8_t>(x);
            log[x] = static_cast<std::uint8_t>(i);
            // Multiply by the generator (2) with reduction by 0x11D.
            x <<= 1;
            if (x & 0x100) x ^= 0x11D;
        }
        for (int i = kFieldChar; i < 512; ++i) exp[i] = exp[i - kFieldChar];
    }
};

const GFTables& gf() {
    static const GFTables t;
    return t;
}

inline std::uint8_t gf_mul(std::uint8_t a, std::uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return gf().exp[gf().log[a] + gf().log[b]];
}

inline std::uint8_t gf_div(std::uint8_t a, std::uint8_t b) {
    // Caller guarantees b != 0.
    if (a == 0) return 0;
    return gf().exp[(gf().log[a] + kFieldChar - gf().log[b]) % kFieldChar];
}

inline std::uint8_t gf_pow(std::uint8_t x, int power) {
    int idx = (gf().log[x] * power) % kFieldChar;
    if (idx < 0) idx += kFieldChar;
    return gf().exp[idx];
}

inline std::uint8_t gf_inverse(std::uint8_t x) {
    return gf().exp[kFieldChar - gf().log[x]];
}

using Poly = std::vector<std::uint8_t>;

Poly poly_scale(const Poly& p, std::uint8_t x) {
    Poly r(p.size());
    for (std::size_t i = 0; i < p.size(); ++i) r[i] = gf_mul(p[i], x);
    return r;
}

Poly poly_add(const Poly& p, const Poly& q) {
    Poly r(std::max(p.size(), q.size()), 0);
    for (std::size_t i = 0; i < p.size(); ++i) r[i + r.size() - p.size()] = p[i];
    for (std::size_t i = 0; i < q.size(); ++i) r[i + r.size() - q.size()] ^= q[i];
    return r;
}

Poly poly_mul(const Poly& p, const Poly& q) {
    if (p.empty() || q.empty()) return {};
    Poly r(p.size() + q.size() - 1, 0);
    for (std::size_t j = 0; j < q.size(); ++j)
        for (std::size_t i = 0; i < p.size(); ++i)
            r[i + j] ^= gf_mul(p[i], q[j]);
    return r;
}

std::uint8_t poly_eval(const Poly& poly, std::uint8_t x) {
    std::uint8_t y = poly[0];
    for (std::size_t i = 1; i < poly.size(); ++i) y = static_cast<std::uint8_t>(gf_mul(y, x) ^ poly[i]);
    return y;
}

// Polynomial division; returns the remainder (length len(divisor)-1).
Poly poly_remainder(const Poly& dividend, const Poly& divisor) {
    Poly out = dividend;
    std::size_t sep = divisor.size() - 1;
    if (out.size() < sep) return Poly(sep, 0);
    std::size_t steps = out.size() - sep;
    for (std::size_t i = 0; i < steps; ++i) {
        std::uint8_t coef = out[i];
        if (coef != 0) {
            for (std::size_t j = 1; j < divisor.size(); ++j)
                if (divisor[j] != 0) out[i + j] ^= gf_mul(divisor[j], coef);
        }
    }
    return Poly(out.end() - static_cast<std::ptrdiff_t>(sep), out.end());
}

// Thrown internally when a codeword has more errors than the code can fix.
struct RSUncorrectable {};

Poly generator_poly(std::size_t nsym) {
    Poly g{1};
    for (std::size_t i = 0; i < nsym; ++i) {
        Poly term{1, gf_pow(2, static_cast<int>(i))};
        g = poly_mul(g, term);
    }
    return g;
}

Poly calc_syndromes(const Poly& msg, std::size_t nsym) {
    Poly synd(nsym + 1, 0);  // synd[0] is a leading zero (used by Forney)
    for (std::size_t i = 0; i < nsym; ++i)
        synd[i + 1] = poly_eval(msg, gf_pow(2, static_cast<int>(i)));
    return synd;
}

bool all_zero(const Poly& p) {
    for (auto v : p)
        if (v != 0) return false;
    return true;
}

// Berlekamp-Massey: derive the error-locator polynomial from the syndromes.
Poly find_error_locator(const Poly& synd, std::size_t nsym) {
    Poly err_loc{1};
    Poly old_loc{1};
    std::size_t synd_shift = synd.size() > nsym ? synd.size() - nsym : 0;
    for (std::size_t i = 0; i < nsym; ++i) {
        std::size_t K = i + synd_shift;
        std::uint8_t delta = synd[K];
        for (std::size_t j = 1; j < err_loc.size(); ++j)
            delta ^= gf_mul(err_loc[err_loc.size() - 1 - j], synd[K - j]);
        old_loc.push_back(0);
        if (delta != 0) {
            if (old_loc.size() > err_loc.size()) {
                Poly new_loc = poly_scale(old_loc, delta);
                old_loc = poly_scale(err_loc, gf_inverse(delta));
                err_loc = new_loc;
            }
            err_loc = poly_add(err_loc, poly_scale(old_loc, delta));
        }
    }
    while (!err_loc.empty() && err_loc.front() == 0) err_loc.erase(err_loc.begin());
    std::size_t errs = err_loc.size() - 1;
    if (errs * 2 > nsym) throw RSUncorrectable{};
    return err_loc;
}

// Chien search: roots of the (reversed) error locator give the error positions.
std::vector<int> find_errors(const Poly& err_loc_rev, std::size_t nmess) {
    std::size_t errs = err_loc_rev.size() - 1;
    std::vector<int> err_pos;
    for (std::size_t i = 0; i < nmess; ++i) {
        if (poly_eval(err_loc_rev, gf_pow(2, static_cast<int>(i))) == 0)
            err_pos.push_back(static_cast<int>(nmess) - 1 - static_cast<int>(i));
    }
    if (err_pos.size() != errs) throw RSUncorrectable{};
    return err_pos;
}

Poly find_errata_locator(const std::vector<int>& coef_pos) {
    Poly e_loc{1};
    for (int p : coef_pos) {
        Poly factor = poly_add(Poly{1}, Poly{gf_pow(2, p), 0});
        e_loc = poly_mul(e_loc, factor);
    }
    return e_loc;
}

Poly find_error_evaluator(const Poly& synd, const Poly& err_loc, std::size_t nsym) {
    Poly prod = poly_mul(synd, err_loc);
    Poly divisor(nsym + 2, 0);
    divisor[0] = 1;  // x^(nsym+1)
    return poly_remainder(prod, divisor);
}

// Forney algorithm: compute and apply error magnitudes at the found positions.
void correct_errata(Poly& msg, const Poly& synd, const std::vector<int>& err_pos) {
    std::vector<int> coef_pos;
    coef_pos.reserve(err_pos.size());
    for (int p : err_pos) coef_pos.push_back(static_cast<int>(msg.size()) - 1 - p);

    Poly err_loc = find_errata_locator(coef_pos);

    Poly synd_rev(synd.rbegin(), synd.rend());
    Poly err_eval = find_error_evaluator(synd_rev, err_loc, err_loc.size() - 1);
    std::reverse(err_eval.begin(), err_eval.end());

    std::vector<std::uint8_t> X;
    X.reserve(coef_pos.size());
    for (int cp : coef_pos) {
        int l = kFieldChar - cp;
        X.push_back(gf_pow(2, -l));
    }

    Poly E(msg.size(), 0);
    for (std::size_t i = 0; i < X.size(); ++i) {
        std::uint8_t Xi = X[i];
        std::uint8_t Xi_inv = gf_inverse(Xi);

        std::uint8_t err_loc_prime = 1;
        for (std::size_t j = 0; j < X.size(); ++j) {
            if (j != i)
                err_loc_prime = gf_mul(err_loc_prime,
                                       static_cast<std::uint8_t>(1 ^ gf_mul(Xi_inv, X[j])));
        }
        if (err_loc_prime == 0) throw RSUncorrectable{};

        Poly err_eval_rev(err_eval.rbegin(), err_eval.rend());
        std::uint8_t y = poly_eval(err_eval_rev, Xi_inv);
        y = gf_mul(Xi, y);

        E[err_pos[i]] = gf_div(y, err_loc_prime);
    }
    msg = poly_add(msg, E);
}

}  // namespace

ReedSolomon::ReedSolomon(std::size_t parity_len) : parity_len_(parity_len) {
    if (parity_len_ == 0 || parity_len_ > 254) throw Error("invalid RS parity length");
}

Bytes ReedSolomon::encode(const Bytes& data) const {
    if (data.size() > max_data_len()) throw Error("RS data block too large");
    Poly gen = generator_poly(parity_len_);
    Poly msg_out(data.size() + parity_len_, 0);
    std::copy(data.begin(), data.end(), msg_out.begin());
    for (std::size_t i = 0; i < data.size(); ++i) {
        std::uint8_t coef = msg_out[i];
        if (coef != 0) {
            for (std::size_t j = 1; j < gen.size(); ++j)
                msg_out[i + j] ^= gf_mul(gen[j], coef);
        }
    }
    std::copy(data.begin(), data.end(), msg_out.begin());  // restore data part
    return msg_out;
}

bool ReedSolomon::decode(const Bytes& codeword, Bytes& out) const {
    if (codeword.size() <= parity_len_) return false;
    Poly msg(codeword.begin(), codeword.end());
    Poly synd = calc_syndromes(msg, parity_len_);
    if (all_zero(synd)) {
        out.assign(msg.begin(), msg.end() - static_cast<std::ptrdiff_t>(parity_len_));
        return true;
    }
    try {
        Poly err_loc = find_error_locator(synd, parity_len_);
        Poly err_loc_rev(err_loc.rbegin(), err_loc.rend());
        std::vector<int> err_pos = find_errors(err_loc_rev, msg.size());
        correct_errata(msg, synd, err_pos);
        Poly check = calc_syndromes(msg, parity_len_);
        if (!all_zero(check)) return false;
    } catch (const RSUncorrectable&) {
        return false;
    }
    out.assign(msg.begin(), msg.end() - static_cast<std::ptrdiff_t>(parity_len_));
    return true;
}

}  // namespace emcast
