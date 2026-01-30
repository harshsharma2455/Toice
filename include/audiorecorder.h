#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QObject>
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QVector>
#include <QIODevice>
#include <QDebug>

class AudioRecorder : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder();
    
    void start();
    void stop();
    void setDevice(const QAudioDevice &device);
    static QList<QAudioDevice> availableDevices();
    QVector<float> getRecordedAudio();

signals:
    void audioAvailable(const QVector<float> &data);
    void audioLevel(float level);
    void frequencyBands(const QVector<float> &bands); // 6 frequency bands

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    QAudioSource *audioSource = nullptr;
    QAudioFormat format;
    QAudioDevice currentDevice;
    QVector<float> m_recordedAudio;
};

#endif // AUDIORECORDER_H
