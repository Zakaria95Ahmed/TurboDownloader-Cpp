/**
 * @file SpeedCalculator.cpp
 * @brief Speed calculation and ETA estimation utilities
 */

#include "openidm/engine/Types.h"
#include <deque>
#include <numeric>
#include <cmath>

namespace OpenIDM {

/**
 * @class SpeedCalculator
 * @brief Calculates smoothed download speed and ETA
 */
class SpeedCalculator {
public:
    SpeedCalculator(size_t sampleCount = Constants::SPEED_HISTORY_SIZE)
        : m_maxSamples(sampleCount)
    {
    }
    
    /**
     * @brief Add a speed sample
     * @param bytesPerSecond Current speed measurement
     */
    void addSample(SpeedBps bytesPerSecond) {
        m_samples.push_back(bytesPerSecond);
        
        if (m_samples.size() > m_maxSamples) {
            m_samples.pop_front();
        }
    }
    
    /**
     * @brief Get smoothed average speed
     * @return Average speed in bytes/second
     */
    SpeedBps averageSpeed() const {
        if (m_samples.empty()) {
            return 0.0;
        }
        
        double sum = std::accumulate(m_samples.begin(), m_samples.end(), 0.0);
        return sum / m_samples.size();
    }
    
    /**
     * @brief Get exponentially smoothed speed
     * @param alpha Smoothing factor (0-1, higher = more weight to recent)
     * @return Smoothed speed in bytes/second
     */
    SpeedBps exponentialSmoothedSpeed(double alpha = Constants::ETA_SMOOTHING_FACTOR) const {
        if (m_samples.empty()) {
            return 0.0;
        }
        
        double smoothed = m_samples.front();
        
        for (size_t i = 1; i < m_samples.size(); ++i) {
            smoothed = alpha * m_samples[i] + (1.0 - alpha) * smoothed;
        }
        
        return smoothed;
    }
    
    /**
     * @brief Calculate ETA based on remaining bytes
     * @param remainingBytes Bytes left to download
     * @return Estimated time remaining in milliseconds
     */
    Duration calculateETA(ByteCount remainingBytes) const {
        SpeedBps speed = exponentialSmoothedSpeed();
        
        if (speed <= 0 || remainingBytes <= 0) {
            return Duration{-1};  // Unknown
        }
        
        double seconds = static_cast<double>(remainingBytes) / speed;
        return Duration{static_cast<int64_t>(seconds * 1000)};
    }
    
    /**
     * @brief Get current (instantaneous) speed
     * @return Last recorded speed
     */
    SpeedBps currentSpeed() const {
        return m_samples.empty() ? 0.0 : m_samples.back();
    }
    
    /**
     * @brief Clear all samples
     */
    void reset() {
        m_samples.clear();
    }
    
    /**
     * @brief Get number of samples
     */
    size_t sampleCount() const {
        return m_samples.size();
    }
    
private:
    std::deque<SpeedBps> m_samples;
    size_t m_maxSamples;
};

} // namespace OpenIDM
