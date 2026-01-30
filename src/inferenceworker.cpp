#include "inferenceworker.h"
#include "databasemanager.h"
#include <QCoreApplication>
#include <iostream>

InferenceWorker::InferenceWorker(QObject *parent) : QThread(parent)
{
    struct whisper_context_params cparams = whisper_context_default_params();
    QString modelPath = DatabaseManager::instance().getSetting("model_path");
    
    // Fallback logic for development or manual folder placement
    if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
        modelPath = QCoreApplication::applicationDirPath() + "/models/ggml-base.en.bin";
    }
    
    qDebug() << "Loading model from:" << modelPath;
    ctx = whisper_init_from_file_with_params(modelPath.toStdString().c_str(), cparams);
    
    if (!ctx) {
        qCritical() << "Failed to initialize whisper context";
    } else {
        qDebug() << "Whisper initialized successfully";
    }
}

InferenceWorker::~InferenceWorker()
{
    stop();
    wait();
    if (ctx) whisper_free(ctx);
}

void InferenceWorker::addAudio(const QVector<float> &audio)
{
    // We still keep this to avoid breaking existing signatures, 
    // but focus on requestFinalTranscription for actual processing.
}

void InferenceWorker::clear()
{
    QMutexLocker locker(&mutex);
    m_shouldClear = true;
    m_hasFinalRequest = false;
    m_finalProcessingBuffer.clear();
}

void InferenceWorker::stop()
{
    m_stop = true;
}

void InferenceWorker::requestFinalTranscription(const QVector<float> &audio)
{
    QMutexLocker locker(&mutex);
    m_finalProcessingBuffer = audio;
    m_hasFinalRequest = true;
}

void InferenceWorker::reloadModel(const QString &modelPath)
{
    QMutexLocker locker(&mutex);
    qDebug() << "Reloading model from:" << modelPath;
    
    if (ctx) {
        whisper_free(ctx);
        ctx = nullptr;
    }
    
    if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
        qCritical() << "Model file not found:" << modelPath;
        return;
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    ctx = whisper_init_from_file_with_params(modelPath.toStdString().c_str(), cparams);
    
    if (!ctx) {
        qCritical() << "Failed to initialize whisper context from" << modelPath;
    } else {
        qDebug() << "Whisper re-initialized successfully";
        DatabaseManager::instance().setSetting("model_path", modelPath);
    }
}

void InferenceWorker::run()
{
    while (!m_stop) {
        bool processFinal = false;
        QVector<float> bufferToProcess;
        
        {
            QMutexLocker locker(&mutex);
            if (m_hasFinalRequest) {
                bufferToProcess = m_finalProcessingBuffer;
                m_finalProcessingBuffer.clear();
                m_hasFinalRequest = false;
                processFinal = true;
            }
        }
        
        if (processFinal && !bufferToProcess.isEmpty() && ctx) {
             qDebug() << "Processing final transcription for" << bufferToProcess.size() / 16000.0 << "seconds of audio";
             
             whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
             wparams.print_progress = false;
             wparams.print_special = false;
             wparams.print_realtime = false;
             wparams.print_timestamps = false;
             wparams.translate = false;
             wparams.language = "en";
             wparams.n_threads = qMin(4, QThread::idealThreadCount()); // Limit to 4 threads to prevent Flatpak/Sandbox contention
             wparams.offset_ms = 0;
            
             if (whisper_full(ctx, wparams, bufferToProcess.data(), bufferToProcess.size()) != 0) {
                 qCritical() << "failed to process audio";
                 emit finalResultReady("");
             } else {
                 const int n_segments = whisper_full_n_segments(ctx);
                 QString fullText = "";
                 for (int i = 0; i < n_segments; ++i) {
                     const char *text = whisper_full_get_segment_text(ctx, i);
                     fullText += QString::fromUtf8(text);
                 }
                 qDebug() << "Final Result ready:" << fullText.trimmed();
                 emit finalResultReady(fullText.trimmed());
             }
        }
        
        QThread::msleep(100);
    }
}
