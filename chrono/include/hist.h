//
// AUDIO TEMPORAL STRETCHER CODE LICENSE
//
// PERMISION NOTICE
//
// Copyright © 2022 Audinate Pty Ltd ACN 120 828 006 (Audinate). All rights reserved. 
//
//
// 1.   Subject to the terms and conditions of this Licence, Audinate hereby grants you a worldwide, non-exclusive, 
//      no-charge, royalty free licence to copy, modify, merge, publish, redistribute, sublicense, and/or sell the 
//      Software, provided always that the following conditions are met: 
//      1.1.    the Software must accompany, or be incorporated in a licensed Audinate product, solution or offering 
//              or be used in a product, solution or offering which requires the use of another licensed Audinate 
//              product, solution or offering. The Software is not for use as a standalone product without any 
//              reference to Audinate’s products;
//      1.2.    the Software is provided as part of example code and as guidance material only without any warranty 
//              or expectation of performance, compatibility, support, updates or security; and
//      1.3.    the above copyright notice and this License must be included in all copies or substantial portions 
//              of the Software, and all derivative works of the Software, unless the copies or derivative works are 
//              solely in the form of machine-executable object code generated by the source language processor.
//
// 2.   TO THE EXTENT PERMITTED BY APPLICABLE LAW, THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
//      XPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
//      PURPOSE AND NONINFRINGEMENT. 
//
// 3.   TO THE FULLEST EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL AUDINATE BE LIABLE ON ANY LEGAL THEORY 
//      (INCLUDING, WITHOUT LIMITATION, IN AN ACTION FOR BREACH OF CONTRACT, NEGLIGENCE OR OTHERWISE) FOR ANY CLAIM, 
//      LOSS, DAMAGES OR OTHER LIABILITY HOWSOEVER INCURRED.  WITHOUT LIMITING THE SCOPE OF THE PREVIOUS SENTENCE THE 
//      EXCLUSION OF LIABILITY SHALL INCLUDE: LOSS OF PRODUCTION OR OPERATION TIME, LOSS, DAMAGE OR CORRUPTION OF 
//      DATA OR RECORDS; OR LOSS OF ANTICIPATED SAVINGS, OPPORTUNITY, REVENUE, PROFIT OR GOODWILL, OR OTHER ECONOMIC 
//      LOSS; OR ANY SPECIAL, INCIDENTAL, INDIRECT, CONSEQUENTIAL, PUNITIVE OR EXEMPLARY DAMAGES, ARISING OUT OF OR 
//      IN CONNECTION WITH THIS AGREEMENT, ACCESS OF THE SOFTWARE OR ANY OTHER DEALINGS WITH THE SOFTWARE, EVEN IF 
//      AUDINATE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH CLAIM, LOSS, DAMAGES OR OTHER LIABILITY.
//
// 4.   APPLICABLE LEGISLATION SUCH AS THE AUSTRALIAN CONSUMER LAW MAY APPLY REPRESENTATIONS, WARRANTIES, OR CONDITIONS, 
//      OR IMPOSES OBLIGATIONS OR LIABILITY ON AUDINATE THAT CANNOT BE EXCLUDED, RESTRICTED OR MODIFIED TO THE FULL 
//      EXTENT SET OUT IN THE EXPRESS TERMS OF THIS CLAUSE ABOVE "CONSUMER GUARANTEES".  TO THE EXTENT THAT SUCH CONSUMER 
//      GUARANTEES CONTINUE TO APPLY, THEN TO THE FULL EXTENT PERMITTED BY THE APPLICABLE LEGISLATION, THE LIABILITY OF 
//      AUDINATE UNDER THE RELEVANT CONSUMER GUARANTEE IS LIMITED (WHERE PERMITTED AT AUDINATE’S OPTION) TO ONE OF 
//      FOLLOWING REMEDIES OR SUBSTANTIALLY EQUIVALENT REMEDIES:
//      4.1.    THE REPLACEMENT OF THE SOFTWARE, THE SUPPLY OF EQUIVALENT SOFTWARE, OR SUPPLYING RELEVANT SERVICES AGAIN;
//      4.2.    THE REPAIR OF THE SOFTWARE;
//      4.3.    THE PAYMENT OF THE COST OF REPLACING THE SOFTWARE, OF ACQUIRING EQUIVALENT SOFTWARE, HAVING THE RELEVANT 
//              SERVICES SUPPLIED AGAIN, OR HAVING THE SOFTWARE REPAIRED.
//
// 5.   This License does not grant any permissions or rights to use the trade marks (whether registered or unregistered), 
//      the trade names, or product names of Audinate. 
//
// 6.   If you choose to redistribute or sell the Software you may elect to offer support, maintenance, warranties, 
//      indemnities or other liability obligations or rights consistent with this License. However, you may only act on 
//      your own behalf and must not bind Audinate. You agree to indemnify and hold harmless Audinate, and its affiliates 
//      form any liability claimed or incurred by reason of your offering or accepting any additional warranty or additional 
//      liability. 
//
//  hist.h
//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HISTOGRAM
//
// This is designed to be simple and lean.  The histogram range is specificed by the bin centres.
// The basic structure
//
// Take care with this depending on whether the item being accumulated is continuous or discrete.  For example
// a range of integers from 0:10 would be best captured with bin0=0, binN=10 and importantly bins=11.
//
// However, we run into trouble again considering integers from 0:20 and 11 bins.  In this case, the odd
// integers fall exactly on a bin boundary, so without resonance, the bin centres are wrong (biased)
// or else the user should set the bins as  bin0=0.5, binN=20.5 and use 11 bins, or 0.5..18.5 and 11 bins.
// If the centres are required to be integer, then the best would be bin0=0, binN=21, bins=8
//
// This is particularly relevant when binning discrete valued items, like length of packet losses.  As
// a rule, to avoid bias, if the bin centres are integers, ensure the effective bin width is an odd
// integer.   That is BinN = Bin0 + (2n+1)*(bins-1).
//
// The binning is simply as follows
// width = (BinN - Bin0)/(bins-1)
// BIN 0   [ bin0-width/2  ... bin0         ... bin0+width/2  )
// BIN 1   [ bin1-width/2  ... bin0+1*width ... bin1+width/2  )
// ...
// BIN N   [ binN-width/2  ... bin0+N*width ... binN+width/2  ]    Extremum included in binning
//
// Note that N = bins-1 and the full range covered before overflow is bins * width, however the range
// from centre to centre is (bins-1)*width.  This creates simple computation for the bin centres as
// bin0+n*width and the edges are n+-1/2
//
// Overflows will be accumulated into the first and last bins.  So if you want to track underflows separately
// then extend the range to add two more bins and suiable centres just past your desired uncorrupted range.
//
// If defined to use floats internally, bins can store fractional values, so you can accumulate probabilistic
// or continuous valued events and even negative events.  Counter values must fit into specified int size, or if
// using floats, then discrete events can be accumulated in each bin until 2^24-1 due to mantissa precision.
//
// As an option, the histogram may be accumulated including stochastic resonance.  This is useful to remove the
// bias from sparse disrtibutions within any bin.  Stockastic resonance is the addition of a random uniformly
// distributed offset between -0.5 and +0.5 bin.  This helps to linearize the quantization of the measured value
// and therefore eliminate bias in the statistics derived from the histogram.
//
// The design is based around the idea that fast running agent is accumulating into the histogram,
// whilst other thread(s) can be resetting and changing the range.  It does not use mutex to allow usage
// in real time threads.  It slightly vulnerable when increasing the number of bins after initial
// config, though this is managed by cycling the old bin pointer and exiling only after a second change.
// The histogram bins are only incremented by one thread, and events are atomic.
//
// A read/copy of a histogram bins always represents a valid sample of things, unless another user clears
// the histogram during a read.  The precise sum and sum^2 may be inconsistent if an add occurs duing
// use of them.  If either of these concerns is critical, then use the consistent 'copy' prior to getting
// any statistics, or explicitly make sure a *(volatile int*)&p->configs does not change across calculation.
//
// The system is designed to be very statistically unbiased - such that when there is not over flow in the
// bin range then the true mean and variance are unbiased and efficient to within sample size and the
// error introduced by the bin width - provided stochastic resonance is enabled.
//

#pragma once
#include <stdint.h>
#include <time.h>

namespace Audinate { namespace hist {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STORAGE TYPE FOR EACH BIN
// Generally will be an int.  Can be char if memory sensitive or float if accumulating continuous measures.
//
typedef uint32_t HistType;

enum HistFlag : int
{
    NONE    = 0x00000000, // Default of no flags
    DITHER  = 0x00000001, // The entries are dithered before being placed into bins - removes bias but lowers resolution
    COUNTER = 0x00000002, // The value arguments are strictly integers
    LOGX    = 0x00000004, // The bins are spaced logarithmically in the x axis

};
inline HistFlag operator|(HistFlag a, HistFlag b) { return (HistFlag)((int)a | (int)b); };
inline HistFlag operator&(HistFlag a, HistFlag b) { return (HistFlag)((int)a & (int)b); };

enum HistTextOption
{
    PLAIN    = 0x00000000,
    X_LABEL  = 0x00000001,
    Y_LABEL  = 0x00000002,
    LOGY     = 0x00000004,
    NO_TITLE = 0x00000008,
    MEAN     = 0x00000100,
    STD      = 0x00000200,
    MEDIAN   = 0x00000400,
    MODE     = 0x00000800,
    MAX      = 0x00001000,
    MIN      = 0x00002000,
    TOTAL    = 0x00004000,
    SUM      = 0x00008000,
    STATS    = 0x0000FF00,
    CULL     = 0x80000000,
    PERCENT  = 0x00010000,
    CUM_PER  = 0x00020000
};

inline HistTextOption operator|(HistTextOption a, HistTextOption b) { return (HistTextOption)((int)a | (int)b); };
inline HistTextOption operator&(HistTextOption a, HistTextOption b) { return (HistTextOption)((int)a & (int)b); };
inline HistTextOption operator~(HistTextOption a) { return (HistTextOption)(~(int)a); };

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HISTOGRAM CONFIGURATION MANAGEMENT AND UPDATE
//
struct hist_t;

class Histogram
{
  public:
    Histogram();
    Histogram(float bin0, float binN, int bins=100, HistFlag flags=HistFlag::DITHER, const char *name = nullptr) : Histogram()
    { this->config(bin0, binN, bins, flags, name); };

    ~Histogram();

    bool config(
        float       bin0,                     // The centre of the first bin
        float       binN,                     // The centre of the last bin
        int         bins  = 100,              // The number of bins (N+1)
        HistFlag    flags = HistFlag::DITHER, // Flags for creation and update mode control
        const char *name  = nullptr           // Optional name)
    );
    void reset();                      // Reset without changing the config
    void add(float x, HistType n = 1); // Low overhead call for each event

    bool reconfig(
        float       bin0,
        float       binN,
        int         bins,
        HistFlag    flags,
        uint32_t *  bin, // Existing set of data to work from
        const char *name = nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // DATA EXTRACTION FUNCTIONS
    //
    // Functions that return data from the histogram.
    //
    // Note that a histogram is a quantized and potentially biased representation of statistical data.  Quantization injects
    // noise, and if stochastic resonance is not enabled, it also creates bias.  Bias will also occur if any saturation or
    // overflow has happened in the sense of the extreme bins being populated.  For that reason, we provide methods that
    // allow removal of the extreme bins (cull) to calculate moments without outliers.  Since entries in the extreme bins
    // is probably but not certain evidence of saturation, a test of true saturation is to compare the mean from the bins
    // with the true mean.  This is only valid if the core logging has a true mean (almost always), because the mean
    // function call will default to the histogram mean (with culling) if true mean is not available, and then the test is
    // again the same as there being data in the edge bins.
    //
    // Much of this complexity is abstracted from the user in these functions which all endeavour to return the most
    // unbiased value available given the structure and logging underneath.
    //

    HistFlag flags() const;                 // Things like dither enabled and log scaling options
    HistType n(bool cull = false) const;    // Either the internal cumulative total (N) or the bin sum excluding edges
    float    sum(bool cull = false) const;  // Either the internal cumulative sum (sumX) or the bin cumulative sum excluding edges
    float    mean() const;                  // Most unbiased mean - sumX / N
    float    std() const;                   // Most unbiased standard deviation - sqrt(sumX2/N - u^2)
    float    max() const;                   // X value of right edge of maximum non zero bin
    float    min() const;                   // X value of left  edge of minimum non zero bin
    HistType peak(bool cull = false) const; // Peak value across all bins - useful for scaling

    float binMean(bool cull = false) const;                // Option to remove outlier bins (first and last)
    float binStd(bool cull = false) const;                 // Option to remove outlier bins (first and last)
    float median(bool cull = false) const;                 // Median
    float percent(float percent, bool cull = false) const; // Median is 50% percentile
    float mode(bool cull = false) const;                   // Location of the peak using

    float    binCenter(int bin) const; // Return the centre value of a bin
    float    binWidth() const;         // Return the width of a bin
    int      bins() const;             // Return number of bins
    HistType bin(int bin) const;       // Return the bin value - not allowing direct access to buffer

    bool text(                                                     // Create a text representation of a histogram
        int            barHeight,                                  // Will be this many characters high
        char *         str,                                        // Buffer must be barHeight*(p->bins+2) or barHeight*(p->bins+4) if yY_LABEL is set
        HistTextOption flags   = X_LABEL | Y_LABEL | TOTAL | MEAN, // Set the flags for how to draw
        int            yMax    = 0,                                // Fix the vertical max scale if > 0
        const char *   symbols = nullptr                           // Set of symbols to use - cast 0,1 or 2 for defaults
    ) const;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Versions
    static unsigned int versionMajor();
    static unsigned int versionMinor();
    static unsigned int versionPatch();
    static const char * versionSuffix();
    static const char * versionHash();
    static const char * versionFull();

  private:
    char mData[64 + 4 + 4 + 4 + 4 + 4 + 4 + 101 * 4 + 4 + 4 + 4 + 4 + 4 + 4];
    // @Alan - is there a convenient way of doing this without exposing struct?
    // Note that the config has an assert to ensure this is correct size
};

}} // namespace Audinate::hist

//
// Copyright © 2022 Audinate Pty Ltd ACN 120 828 006 (Audinate). All rights reserved. 
//