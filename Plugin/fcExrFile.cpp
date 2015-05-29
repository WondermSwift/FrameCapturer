#include "pch.h"
#include "FrameCapturer.h"
#include "fcGraphicsDevice.h"

#ifdef fcSupportEXR

class fcExrContext
{
public:
    fcExrContext(fcExrConfig &conf);
    ~fcExrContext();
    void writeFrame(void *tex, int width, int height, fcETextureFormat fmt, const char *path);

private:
    void writeFrameTask(const std::string &raw_frame, fcETextureFormat fmt, const std::string &path);

private:
    int m_magic; //  for debug
    fcExrConfig m_conf;
    std::vector<std::string> m_raw_frames;
    tbb::task_group m_tasks;
    int m_frame;
    std::atomic_int m_active_task_count;

};

fcExrContext::fcExrContext(fcExrConfig &conf)
    : m_magic(fcE_ExrContext)
    , m_conf(conf)
{
    m_raw_frames.resize(m_conf.max_active_tasks);
}

fcExrContext::~fcExrContext()
{
    m_tasks.wait();
}

void fcExrContext::writeFrameTask(const std::string &raw_frame, fcETextureFormat fmt, const std::string &path)
{
    // todo: 

}

void fcExrContext::writeFrame(void *tex, int width, int height, fcETextureFormat fmt, const char *_path)
{
    if (m_active_task_count >= m_conf.max_active_tasks)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (m_active_task_count >= m_conf.max_active_tasks)
        {
            m_tasks.wait();
        }
    }
    int frame = m_frame++;
    std::string& raw_frame = m_raw_frames[frame % m_conf.max_active_tasks];
    raw_frame.resize(width*height * fcGetPixelSize(fmt));

    // �t���[���o�b�t�@�̓��e�擾
    fcGetGraphicsDevice()->copyTextureData(&raw_frame[0], raw_frame.size(), tex, width, height, fmt);

    // .exr �����o���^�X�N�� kick
    std::string path = _path;
    ++m_active_task_count;
    m_tasks.run([this, &raw_frame, fmt, path](){
        writeFrameTask(raw_frame, fmt, path);
        --m_active_task_count;
    });
}



#ifdef fcDebug
#define fcCheckContext(v) if(v==nullptr || *(int*)v!=fcE_ExrContext) { fcBreak(); }
#else  // fcDebug
#define fcCheckContext(v) 
#endif // fcDebug

fcCLinkage fcExport fcExrContext* fcExrCreateContext(fcExrConfig *conf)
{
    return new fcExrContext(*conf);
}

fcCLinkage fcExport void fcExrDestroyContext(fcExrContext *ctx)
{
    fcCheckContext(ctx);
    delete ctx;
}

fcCLinkage fcExport void fcExrWriteFile(fcExrContext *ctx, void *tex, int width, int height, fcETextureFormat fmt, const char *path)
{
    fcCheckContext(ctx);
    ctx->writeFrame(tex, width, height, fmt, path);
}

#endif // fcSupportEXR
