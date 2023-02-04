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
// ats.h
//


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AUDINATE AUDIO TEMPORAL STRETCHER
//
// The universal rescue bed for bad audio timing - use with caution
//
// Attempting to wrap an abstraction around a module that can produce the best audio it can when needed using
// as much information as possible, and hiding the details (other than configuring the mode to use).
//
// ...

#pragma once
#include "chrono.h" // Event timing facility
#include <cmath>
#include <cstdint>
#include <cstdio>

typedef float AtsData;
#ifndef ATS_BUFFER_SIZE_LOG2
#define ATS_BUFFER_SIZE_LOG2 (12)
#define ATS_BUFFER_SIZE      (1 << ATS_BUFFER_SIZE_LOG2) // Audio buffer size per channel. Compile time fixed.
#endif

namespace Audinate { namespace ats {

using namespace Audinate::hist;
using namespace Audinate::chrono;

struct ats_t;

typedef enum Mode : int
{
    ATS_ZERO_ORDER_HOLD = 0x00000000, // Useful mode for debugging - just skip samples, no effort to conceal

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INTERPOLATION METHODS - Creating samples at fractional times - complexity hit at POP time
    //
    // These are in increasing complexity
    //

    ATS_INTERP_MASK    = 0x0000000F, // This mask used to determine if any sample interpolation is enabled
    ATS_INTERP_HOLD    = 0x00000000, // Zero order hold
    ATS_INTERP_LINEAR  = 0x00000001, // Simple two point linear interpolation
    ATS_INTERP_SPLINE3 = 0x00000002, // Four sample cubic interpolation
    ATS_INTERP_SPLINE5 = 0x00000003, // Six sample quintic interpolation

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INPUT FILTERING METHODS - Improve audio precision - complexity hit at PUSH time
    //
    // General resampling is achieved by a selected combination of three possible stages.  Used individually or
    // collectively they allow for a trade off of complexity, noise, and loading push or pop.
    // 1. IIR filter on the input can be used ensure signal is comfortably sub nyquist
    //    This is a small load on the Push, and avoids aliasing distortion at the cost of high frequency
    //    signal attenuation and some non linear phase filtering.  This should ensure there is no
    //    signal above nyquist of the lowest rate (input or output).
    // 2. FIR filter on the input to over sample.
    //    This is a large load on the Push, and can asymptotically eliminate aliasing with increasing length.
    //    Filters will either add delay or else not be linear phase.
    // 3. Interpolation on the output to provide approximate subsample delay
    //    This is a load on the Pop that increases with the order of the interpolation
    //
    // All of these implement well with small frame sizes.  This general design philosophy is discussed in
    // http://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
    //
    // As a general ceiling of capability for the base implmenetation a 2X over sample, with quintic spline
    // will achieve SNR > 100dB - suitable for <1ppm tracking noise and precision characterization.
    //
    // A single biquad with cubic spline can do ~60dB which is generally sufficient for any consumer
    // application and below perceptual limits on normal program material.

    ATS_FILTER_MASK    = 0x000000F0, // The mask used to determine if anything to be done
    ATS_FILTER_BIQUAD  = 0x00000010, // Run a default 2nd order IIR filter on input - rate adjusted
    ATS_FILTER_BIQUAD2 = 0x00000020, // Run a default 4th order IIR filter on input - rate adjusted
    ATS_FILTER_FIR2X   = 0x00000040, // Run a 2X oversampling at input - halves effective buffer size
    ATS_FILTER_FIR     = 0x00000080, // Run a custom FIR

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ADDITIONAL MODE FLAGS FOR TESTING AND WIDER APPLICATIONS
    //

    ATS_TRACKING_OFF = 0x10000000 // Prevent the tracking from being called during a push - fixed rate

} Mode;
inline Mode operator|(Mode a, Mode b) { return (Mode)((int)a | (int)b); };
inline Mode operator&(Mode a, Mode b) { return (Mode)((int)a & (int)b); };
inline Mode operator~(Mode a) { return (Mode)(~(int)a); };

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONFIG BLOCK
//
// This is a good place to look at the overview of the sorts of things the ATS can do.  The config blocks is
// where you would setup everything about the mode of operation and how it handles interpolation, under-runs,
// overruns, missed data and tracking towards the ideal target (skips and drops).
//
// A set of sensible defaults are provided, so that for the majority use case (48kHz 1:1 with a control loop)
// the initialization can just be the default.
//
typedef struct Config
{
    int              channels      = 2;                   // Number of channels to run on - note data is interleaved so SIMD will be across channels
    static const int bufferSamples = ATS_BUFFER_SIZE;     // This is compile time fixed, but accessible in the config (must be power of 2)
    Mode             mode          = ATS_INTERP_SPLINE5;  // General mode of operation flags with a sensible default
    float            inRate        = 48000.0F;            // The expected input sample rate
    float            outRate       = 48000.0F;            // The expected output sample rate
    int              filterPush    = 200;                 // Window size for filtering the push offsets - at push rate
    int              filterPop     = 200;                 // Window size for filtering the pop  offsets - at pop rate
    int              trackTarget   = ATS_BUFFER_SIZE / 4; // The desired latency to track // FIXME: set better default value
    int              trackRange    = 0;                   // Drift before a reset			samples				0 is off
    float            trackKp       = 2.0F;                // Proportional gain 			ppm / samples		1.0 is around 1m to 1 sample at 48kHz
    float            trackKi       = 0.1F;                // Integral gain 				ppm / samples		Typically 1/20 of Ki
    float            trackWarp     = 10.0F;               // Quadratic warp (hysteresis) 	SAMPLES				Scales down Kp by error/warp
    float            trackRate     = 10.0F;               // Maximum rate of tracking		ppm / second		Smooths the tracking
} Config;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EVENTS
//
// This is a set of events that are logged in histograms.  The chrono class is very lean, so these are tracked
// with near zero overhead.  The value of these in diagnostics justify them being always enabled.

enum Event : int // A set of lean chrono time stamper / counters
{
    PUSH,           // Each push event - can calculate rates if push is always same size
    PUSH_RATE,      // Instantaneous Raw Audio Rate (T/N) adjusted by the callTime
    PUSH_EXEC,      // Time to execute the push call
    POP,            // Each pop event  - also useful for rates if fixed size
    POP_RATE,       // Instantaneous Raw Audio Rate (T/N) adjusted by the callTime
    POP_EXEC,       // Time to execute the pop call
    UNDER_RUN,      // Insufficient data in buffer to complete a pop without extrapolation
    UNDER_RUN_SIZE, // Track the count of the size of the under-runs - this is a counter so use aud_chrono_hist_raw
    OFFSET,         // The control rate offset from outRate/inRate in ppm fine range
    DEPTH,          // Sample of the depth of the buffer at each track call
    LATENCY,        // Sample of finer detail around target latency (subject to observation error)
    TRACK,          // Used to ensure tracking is called, and estimate rate of track call for I control
    EVENTS,
    ALL = EVENTS
};

class Ats
{
  public:
    Ats();
    ~Ats();

    bool          config(Config *config);              // Set parameters = not if channels or buffer size changes, then all buffers are reset
    const Config *getConfig(Config *config = nullptr); // Grab a reference or copy the configuration
    void          push(int samples, int sampleStride, int channelStride, int32_t *data, int64_t callTime = 0);
    void          skip(int samples);                  
    void          pop(int samples, int sampleStride, int channelStride, AtsData *dst, int64_t callTime = 0);
    void          pop(int samples, int sampleStride, int channelStride, int32_t *dst, int64_t callTime = 0);
    int           getDepth();                                    // Get the current buffer depth
    void          setDepth(int depth);                           // Asynchronously set the depth (will not be the exact latency)
    float         getLatency();                                  // Most recent estimate of the latency, time adjusted.  We cannot set this but we can nudge it
    void          trackReset();                                  // Full reset of the tracking
    Chrono *      chrono(Event index);                           // Pointer to the chrono object for a particular event - will create and enable if not already
    void          chronoReset(Event index = ALL);                // Will reset a counter, with the default to reset all of them
    void          chronoDefault(int bins = 101, float T = 0.01); // Set all chronos and counters to a default configuration
    bool          setRate(double rate);                          // Ratio of target output sample rate to input
    double        getRate();                                     //
    void          histogram(Event event, Histogram *h = nullptr);

    void trace(std::FILE *f); // Push a line of tracing information to a stdio

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Versions
    static unsigned int versionMajor();
    static unsigned int versionMinor();
    static unsigned int versionPatch();
    static const char * versionSuffix();
    static const char * versionHash();
    static const char * versionFull();

  private:
    void atsTrack(); // Execute a tracking update - called in Pop
    char mData[10448];
    // @Alan - is there a convenient way of doing this without exposing struct?
    // Note that the config has an assert to ensure this is correct size
};

}} // namespace Audinate::ats

//
// Copyright © 2022 Audinate Pty Ltd ACN 120 828 006 (Audinate). All rights reserved. 
//
