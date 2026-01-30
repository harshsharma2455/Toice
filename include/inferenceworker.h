#ifndef INFERENCEWORKER_H
#define INFERENCEWORKER_H

#include <QObject>
#include <QThread>
#include <QVector>
#include <QMutex>
#include <QQueue>
#include <QDebug>
#include "whisper.h"

class InferenceWorker : public QThread
{
    Q_OBJECT

public:
    explicit InferenceWorker(QObject *parent = nullptr);
    ~InferenceWorker();
    
    void addAudio(const QVector<float> &audio);
    void stop();
    void clear();
    void requestFinalTranscription(const QVector<float> &audio);
    void reloadModel(const QString &modelPath);

signals:
    void transcriptionUpdated(QString text, bool isFinal);
    void finalResultReady(QString text);

protected:
    void run() override;

private:
    struct whisper_context *ctx = nullptr;
    QQueue<float> audioBuffer;
    QMutex mutex;
    bool m_stop = false;
    bool m_shouldClear = false;
    
    QVector<float> m_finalProcessingBuffer;
    bool m_hasFinalRequest = false;
    
    // Parameters
    int sampleRate = 16000;
};

#endif // INFERENCEWORKER_H
