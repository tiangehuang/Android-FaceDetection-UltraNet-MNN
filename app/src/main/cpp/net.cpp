#include "net.h"
#define TAG "net"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
Inference_engine::Inference_engine()
{ }

Inference_engine::~Inference_engine()
{ 
    if ( netPtr != NULL )
	{
		if ( sessionPtr != NULL)
		{
			netPtr->releaseSession(sessionPtr);
			sessionPtr = NULL;
		}

		delete netPtr;
		netPtr = NULL;
	}
}

int Inference_engine::load_param(std::string & file, int num_thread)
{

    if (!file.empty())
    {

        if (file.find(".mnn") != std::string::npos)
        {

	        netPtr = MNN::Interpreter::createFromFile(file.c_str());
            if (nullptr == netPtr) return -1;

            MNN::ScheduleConfig sch_config;
            sch_config.type = (MNNForwardType)MNN_FORWARD_OPENCL;
            //sch_config.type = (MNNForwardType)MNN_FORWARD_CPU;
            if ( num_thread > 0 )sch_config.numThread = num_thread;

            MNN::BackendConfig backendConfig;

            backendConfig.precision = (MNN::BackendConfig::PrecisionMode)2;
            sch_config.backendConfig = &backendConfig;

            sessionPtr = netPtr->createSession(sch_config);
            if (nullptr == sessionPtr) return -1;
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

int Inference_engine::set_params(int srcType, int dstType, 
                                 float *mean, float *scale)
{
    config.destFormat   = (MNN::CV::ImageFormat)dstType;
    config.sourceFormat = (MNN::CV::ImageFormat)srcType;

    ::memcpy(config.mean,   mean,   3 * sizeof(float));
    ::memcpy(config.normal, scale,  3 * sizeof(float));

    config.filterType = (MNN::CV::Filter)(1);
    config.wrap = (MNN::CV::Wrap)(2);

    return 0;
}

// infer
int Inference_engine::infer_img(cv::Mat& img, Inference_engine_tensor& out)
{
    MNN::Tensor* tensorPtr = netPtr->getSessionInput(sessionPtr, nullptr);

    // auto resize for full conv network.
    bool auto_resize = false;
    if ( !auto_resize )
    {
        std::vector<int>dims = { 1, img.channels(), img.rows, img.cols };
        netPtr->resizeTensor(tensorPtr, dims);
        netPtr->resizeSession(sessionPtr);
    }

    std::unique_ptr<MNN::CV::ImageProcess> process(MNN::CV::ImageProcess::create(MNN::CV::BGR, MNN::CV::RGB, config.mean, 3, config.normal, 3));
    process->convert((const unsigned char*)img.data, img.cols, img.rows, img.step[0], tensorPtr);
    netPtr->runSession(sessionPtr);

    for (int i = 0; i < out.layer_name.size(); i++)
    {
        const char* layer_name = NULL;
        if( strcmp(out.layer_name[i].c_str(), "") != 0)
        {
            layer_name = out.layer_name[i].c_str();
        }
        MNN::Tensor* tensorOutPtr = netPtr->getSessionOutput(sessionPtr, layer_name);

        std::vector<int> shape = tensorOutPtr->shape();

        auto tensor = reinterpret_cast<MNN::Tensor*>(tensorOutPtr);

        std::vector <float> destPtr;
        std::unique_ptr<MNN::Tensor> hostTensor(new MNN::Tensor(tensor, tensor->getDimensionType(), true));
        tensor->copyToHostTensor(hostTensor.get());
        tensor = hostTensor.get();

        auto size = tensor->elementSize();
        for(int i=0 ; i < size * sizeof(float); i++ )
            destPtr.push_back(tensorOutPtr->host<float>()[i]);

        out.out_feat.push_back(destPtr);
    }

    return 0;
}
