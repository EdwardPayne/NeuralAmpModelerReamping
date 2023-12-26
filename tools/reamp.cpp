#ifdef __APPLE__
  #include <cstdlib> // For macOS and other Apple platforms
#else
  #include <malloc.h> // For other platforms
#endif

#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <string>

#include <sndfile.h>

#include "NAM/wav.h"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"
std::mutex outputMutex;

void printProgressBar(int current, int total, int width = 40)
{
  float progress = static_cast<float>(current) / total;
  int barWidth = static_cast<int>(progress * width);

  std::cout << "[";
  for (int i = 0; i < width; ++i)
  {
    if (i < barWidth)
    {
      std::cout << "=";
    }
    else
    {
      std::cout << " ";
    }
  }

  std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%\r";
  std::cout.flush();

  // Print a newline when the progress is complete
  if (current == total)
  {
    std::cout << std::endl;
  }
}

void checkInputParameters(int argc, char* argv[])
{
  // Check if the correct number of command-line arguments is provided
  if (argc != 4 && argc != 5)
  {
    std::cerr << "Usage: " << argv[0] << " <model_filename> <input_filename> <output_filename> <threads_count_override>"
              << std::endl;
    exit(1);
  }
}

std::unique_ptr<nam::DSP> loadModel(const char* modelPath)
{
  std::unique_ptr<nam::DSP> model;
  model.reset();
  model = std::move(nam::get_dsp(modelPath));

  if (model == nullptr)
  {
    std::cerr << "Failed to load model\n";
    exit(1);
  }

  return model;
}

SNDFILE* loadSoundfile(const char* filename, int mode, SF_INFO* sfInfo)
{
  // std::cout << "Loading file " << filename << std::endl;

  SNDFILE* filePtr = sf_open(filename, mode, sfInfo);
  if (!filePtr)
  {
    std::cerr << "Error opening file: " << filename << " " << sf_strerror(NULL) << std::endl;
  }

  return filePtr;
}

void doWork(char* modelFileName, char* inputFileName, SNDFILE* outputFilePtr, sf_count_t startSample,
            sf_count_t endSample, int threadId, std::vector<NAM_SAMPLE>& result, std::mutex& mutex,
            sf_count_t bufferSize)
{
  std::unique_ptr<nam::DSP> model = loadModel(modelFileName);

  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);
}


void doWork_old(char* modelFileName, char* inputFileName, SNDFILE* outputFilePtr, sf_count_t startSample,
                sf_count_t endSample, int threadId, std::vector<NAM_SAMPLE>& result, std::mutex& mutex,
                sf_count_t bufferSize)
{


  std::unique_ptr<nam::DSP> model = loadModel(modelFileName);

  // Öppna filen
  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);

  sf_count_t numSamples = (endSample - startSample);
  sf_count_t numBuffers = (numSamples / bufferSize) + 1;
  sf_count_t restBuffer = (numSamples % bufferSize) > 0 ? 1 : 0;
  sf_count_t startPosition = sf_seek(inputFilePtr, startSample, 0);

  std::vector<NAM_SAMPLE> buffer(bufferSize);
  std::vector<NAM_SAMPLE> processedBuffer(bufferSize);

  for (sf_count_t bufferIndex = 0; bufferIndex < numBuffers; bufferIndex++)
  {
    sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);

    if (bytesRead <= 0)
    {
      std::cerr << "t_id " << threadId << " End of file or error " << bytesRead << std::endl;
      // End of file or error
      break;
    }

    // if (processedBuffer.size() != bytesRead)
    // {
    //   std::cout << "Adjusting buffer size from " << processedBuffer.size() << " to " << bytesRead << std::endl;
    //   // processedBuffer.resize(bytesRead);
    //   processedBuffer.reserve(bytesRead);
    //   // buffer.resize(bytesRead);
    //   buffer.reserve(bytesRead);
    // };


    // model->process(buffer.data(), processedBuffer.data(), bytesRead);
    // model->finalize_(bytesRead);

    // std::cout << "buffer length " << bufferIndex << " " << buffer.size() << std::endl;

    for (const NAM_SAMPLE data : buffer)
    {
      // std::cout << "looping " << bufferIndex << std::endl;
      result.push_back(data);
    }

    // for (const auto& element : processedBuffer)
    // {
    //   result.insert(result.end(), element);
    // }
  }

  sf_close(inputFilePtr);
}

int main(int argc, char* argv[])
{
  int bufferSize = 4096;

  nam::activations::Activation::enable_fast_tanh();

  checkInputParameters(argc, argv);

  char* modelFileName = argv[1];
  char* inputFileName = argv[2];
  char* outputFileName = argv[3];
  int threadsOverride = (argc == 5 && atoi(argv[4]) > 0) ? atoi(argv[4]) : 0;

  std::unique_ptr<nam::DSP> model = loadModel(modelFileName);

  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);
  SF_INFO outputInfo = sfInfo; // Copy input file info
  SNDFILE* outputFilePtr = loadSoundfile(outputFileName, SFM_WRITE, &outputInfo);

  if (!inputFilePtr || !outputFilePtr)
  {
    sf_close(inputFilePtr);
    sf_close(outputFilePtr);
    exit(1);
  }

  std::vector<std::thread> threads;
  std::mutex resultMutex;

  sf_count_t totalFrames = sfInfo.frames;
  sf_count_t numChunks = totalFrames / bufferSize;
  sf_count_t numThreadsMax = threadsOverride > 0 ? threadsOverride : std::thread::hardware_concurrency();
  sf_count_t numThreads = numThreadsMax;

  // make sure we have atleast 4 chunks for each thread, decrease threads if we have to, minimum 1 thread.
  while (numThreads > 0 && (numChunks / numThreads) < 4)
  {
    std::cout << "Adjusting threads count=" << numThreads << " chunkdPerThreads=" << (numChunks / numThreads)
              << std::endl;
    if (--numThreads < 1)
    {
      break;
    };
  }

  std::vector<std::vector<NAM_SAMPLE>> resultVectors(numThreads, std::vector<NAM_SAMPLE>());
  sf_count_t chunkSizePerThread = (numChunks / numThreads);

  // Launch threads
  for (int i = 0; i < numThreads; ++i)
  {

    sf_count_t startFrame;
    sf_count_t endFrame;

    // first thread
    if (i == 0)
    {
      startFrame = 0;
      endFrame = chunkSizePerThread * bufferSize;
    }

    // last thread
    else if (i == numThreads - 1)
    {
      startFrame = ((chunkSizePerThread + 1) * i) * bufferSize;
      endFrame = totalFrames;
    }

    // middle threads
    else
    {
      startFrame = ((chunkSizePerThread + 1) * i) * bufferSize;
      endFrame = startFrame + (chunkSizePerThread * bufferSize);
    }

    // execute thread
    threads.emplace_back(doWork, modelFileName, inputFileName, outputFilePtr, startFrame, endFrame, i,
                         std::ref(resultVectors[i]), std::ref(resultMutex), bufferSize);
  }

  // Join threads
  for (auto& thread : threads)
  {
    thread.join();
  }

  for (int i = 0; i < numThreads; ++i)
  {
    std::cout << "t_id " << i << " resultVectors[i].size() = " << resultVectors[i].size() << std::endl;
    sf_count_t writtenSamples = sf_writef_double(outputFilePtr, resultVectors[i].data(), resultVectors[i].size());
    if (writtenSamples != resultVectors[i].size())
    {
      std::cerr << "Error writing to file for thread " << i << std::endl;
    }
  }

  std::cout << "Audio file successfully processed and written." << std::endl;

  sf_close(inputFilePtr);
  sf_close(outputFilePtr);

  exit(0);
}
