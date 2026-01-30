#include "audiorecorder.h"

AudioRecorder::AudioRecorder(QObject *parent) : QIODevice(parent)
{
    currentDevice = QMediaDevices::defaultAudioInput();
    if (currentDevice.isNull()) {
        qWarning() << "No default audio input device found!";
    }
    setDevice(currentDevice);
    open(QIODevice::WriteOnly); // Open the QIODevice after setting up the device
}

AudioRecorder::~AudioRecorder()
{
    stop();
}

QList<QAudioDevice> AudioRecorder::availableDevices()
{
    return QMediaDevices::audioInputs();
}

void AudioRecorder::setDevice(const QAudioDevice &device)
{
    stop(); // Stop any ongoing recording
    currentDevice = device;

    // Clear existing audioSource if any
    if (audioSource) {
        audioSource->deleteLater(); // Delete the old source
        audioSource = nullptr;
    }

    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    if (!currentDevice.isFormatSupported(format)) {
         qWarning() << "Requested format not supported - system may adapt.";
    }

    audioSource = new QAudioSource(currentDevice, format, this);
}

void AudioRecorder::start()
{
    if (!isOpen()) {
        open(QIODevice::WriteOnly);
    }
    
    if (audioSource) {
        m_recordedAudio.clear(); // Clear for new recording
        audioSource->start(this); 
        qDebug() << "Audio recording started (accumulation active)";
    }
}

void AudioRecorder::stop()
{
    if (audioSource) audioSource->stop();
    close();
}

QVector<float> AudioRecorder::getRecordedAudio()
{
    return m_recordedAudio;
}

// QAudioSource calls this to write captured audio into our buffer
qint64 AudioRecorder::writeData(const char *data, qint64 len)
{
    // Convert bytes to float samples
    // Assuming format is Float, 4 bytes per sample
    int sampleCount = len / sizeof(float);
    QVector<float> samples(sampleCount);
    
    // Copy data and calculate overall level
    const float *ptr = reinterpret_cast<const float*>(data);
    float maxAmp = 0.0f;
    for (int i = 0; i < sampleCount; ++i) {
        samples[i] = ptr[i];
        float val = std::abs(ptr[i]);
        if (val > maxAmp) maxAmp = val;
    }
    
    // Simple frequency band approximation using sample ranges
    // Divide samples into 6 bands (low to high frequency)
    QVector<float> bands(6, 0.0f);
    int samplesPerBand = sampleCount / 6;
    
    for (int band = 0; band < 6; band++) {
        float bandMax = 0.0f;
        int startIdx = band * samplesPerBand;
        int endIdx = (band + 1) * samplesPerBand;
        
        for (int i = startIdx; i < endIdx && i < sampleCount; ++i) {
            float val = std::abs(ptr[i]);
            if (val > bandMax) bandMax = val;
        }
        
        // Add some variation based on band index for more natural look
        float multiplier = 1.0f + (band * 0.1f); // Higher bands get slight boost
        bands[band] = bandMax * multiplier;
    }
    
    emit audioLevel(maxAmp);
    emit frequencyBands(bands);
    emit audioAvailable(samples);
    
    // ACCUMULATE EVERYTHING FOR FINAL TRANSCRIPTION
    m_recordedAudio.append(samples);
    
    return len;
}

qint64 AudioRecorder::readData(char *data, qint64 maxlen)
{
    return 0; // We don't read from here
}
