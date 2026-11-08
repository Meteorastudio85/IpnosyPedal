#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// Resampler mono in streaming (sinc con finestra di Kaiser).
// Tutta la memoria viene allocata in prepare(): process() e' real-time safe.
class StreamResampler
{
public:
    static constexpr int kHalf   = 24;          // tap per lato
    static constexpr int kTaps   = kHalf * 2;
    static constexpr int kPhases = 512;

    // initialOffset: sposta la griglia di uscita di una frazione di campione d'ingresso
    // (serve per rendere intera la latenza complessiva su -> giu').
    void prepare (double inRate, double outRate, int maxInputBlock, double initialOffset = 0.0)
    {
        step = inRate / outRate;
        offset = initialOffset;
        const double cutoff = std::min (1.0, outRate / inRate) * 0.91;

        table.assign ((size_t) (kPhases + 1) * kTaps, 0.0f);
        const double beta = 8.0;
        const double i0b  = besselI0 (beta);

        for (int p = 0; p <= kPhases; ++p)
        {
            const double frac = (double) p / kPhases;
            double sum = 0.0;
            float* row = table.data() + (size_t) p * kTaps;

            for (int k = 0; k < kTaps; ++k)
            {
                const double d = (double) (k - kHalf + 1) - frac;
                const double x = cutoff * d;
                const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (M_PI_VAL * x) / (M_PI_VAL * x);
                const double r = d / (double) kHalf;
                const double w = std::abs (r) >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
                const double v = cutoff * sinc * w;
                row[k] = (float) v;
                sum += v;
            }

            for (int k = 0; k < kTaps; ++k)
                row[k] = (float) (row[k] / sum);
        }

        capacity = maxInputBlock + kTaps * 4 + 16;
        buffer.assign ((size_t) capacity, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        count = kTaps;                 // storia iniziale a zero
        pos   = (double) (kHalf - 1) + offset;
    }

    // Massimo numero di campioni prodotti per un blocco di n campioni in ingresso.
    int maxOutputFor (int n) const { return (int) std::ceil ((double) n / step) + 4; }

    // Ritorna il numero di campioni scritti in out.
    int process (const float* in, int n, float* out, int maxOut)
    {
        int produced = 0;
        int inOffset = 0;

        while (inOffset < n)
        {
            const int space = capacity - count;
            const int chunk = std::min (n - inOffset, space);
            std::memcpy (buffer.data() + count, in + inOffset, sizeof (float) * (size_t) chunk);
            count += chunk;
            inOffset += chunk;

            while (produced < maxOut)
            {
                const int base = (int) pos;
                if (base + kHalf >= count)
                    break;

                const double frac = pos - (double) base;
                const double fp = frac * kPhases;
                const int ph = std::min ((int) fp, kPhases - 1);
                const float a = (float) (fp - ph);
                const float* r0 = table.data() + (size_t) ph * kTaps;
                const float* r1 = r0 + kTaps;
                const float* src = buffer.data() + base - kHalf + 1;

                float acc = 0.0f;
                for (int k = 0; k < kTaps; ++k)
                    acc += src[k] * (r0[k] + a * (r1[k] - r0[k]));

                out[produced++] = acc;
                pos += step;
            }

            const int drop = (int) pos - (kHalf - 1);
            if (drop > 0)
            {
                const int keep = count - drop;
                std::memmove (buffer.data(), buffer.data() + drop, sizeof (float) * (size_t) std::max (0, keep));
                count = std::max (0, keep);
                pos -= (double) drop;
            }
        }

        return produced;
    }

private:
    static constexpr double M_PI_VAL = 3.14159265358979323846;

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        const double y = x * x * 0.25;
        for (int k = 1; k < 50; ++k)
        {
            term *= y / ((double) k * (double) k);
            sum += term;
            if (term < sum * 1.0e-12)
                break;
        }
        return sum;
    }

    std::vector<float> table;
    std::vector<float> buffer;
    int capacity = 0;
    int count = 0;
    double pos = 0.0;
    double step = 1.0;
    double offset = 0.0;
};

// Offset da dare al resampler "giu'" (modello -> host) perche' la latenza totale
// host -> modello -> host sia un numero intero di campioni (pulito e pedale in fase).
inline double roundTripDownOffset (double hostRate, double modelRate)
{
    const double su = hostRate / modelRate;                 // passo del resampler "su"
    const double d  = (StreamResampler::kHalf + 1) * (1.0 + su);
    return (d - std::floor (d)) / su;
}

// FIFO circolare semplice (allocata in prepare).
class SampleFifo
{
public:
    void prepare (int size) { data.assign ((size_t) size, 0.0f); clear(); }
    void clear() { readPos = writePos = 0; level = 0; }
    int  available() const { return level; }

    void push (const float* in, int n)
    {
        const int size = (int) data.size();
        for (int i = 0; i < n && level < size; ++i)
        {
            data[(size_t) writePos] = in[i];
            writePos = (writePos + 1) % size;
            ++level;
        }
    }

    void pushZeros (int n)
    {
        const int size = (int) data.size();
        for (int i = 0; i < n && level < size; ++i)
        {
            data[(size_t) writePos] = 0.0f;
            writePos = (writePos + 1) % size;
            ++level;
        }
    }

    // Se non ci sono abbastanza campioni completa con zeri.
    void pop (float* out, int n)
    {
        const int size = (int) data.size();
        for (int i = 0; i < n; ++i)
        {
            if (level > 0)
            {
                out[i] = data[(size_t) readPos];
                readPos = (readPos + 1) % size;
                --level;
            }
            else
            {
                out[i] = 0.0f;
            }
        }
    }

private:
    std::vector<float> data;
    int readPos = 0, writePos = 0, level = 0;
};
