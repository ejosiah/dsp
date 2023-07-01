#include <kfr/all.hpp>


int main(int, char**){
    using namespace kfr;
    const std::string options = "phaseresp=True, log_freq=True, freq_dB_lim=(-160, 10), padwidth=8192";
    constexpr size_t maxorder = 32;
    univector<fbase, 1024> output;

    zpk<fbase> filt                       = iir_lowpass(chebyshev1<fbase>(8, 2), 0.09);
    std::vector<biquad_params<fbase>> bqs = to_sos(filt);
    output                                = biquad<maxorder>(bqs, unitimpulse());
    plot_save("chebyshev1_lowpass8", output,
              options + ", title='8th-order Chebyshev type I filter, lowpass'");

    return 0;
}