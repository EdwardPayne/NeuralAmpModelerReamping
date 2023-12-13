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

#include <sndfile.h>

#include "NAM/wav.h"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"
std::mutex bufferMutex;
std::mutex fileMutex;

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
  if (argc != 4)
  {
    std::cerr << "Usage: " << argv[0] << " <model_filename> <input_filename> <output_filename>" << std::endl;
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

void doWork(char* modelFileName, char* inputFileName, SNDFILE* outputFilePtr, sf_count_t startFrame,
            sf_count_t endFrame, int threadId, std::vector<NAM_SAMPLE>& result, std::mutex& mutex,
            sf_count_t bufferSize)
{
  std::cout << "Thread start id " << threadId << " startFrame=" << startFrame << " endFrame=" << endFrame << std::endl;

  std::unique_ptr<nam::DSP> model = loadModel(modelFileName);

  // Öppna filen
  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);

  std::vector<NAM_SAMPLE> buffer(bufferSize * sfInfo.channels);
  std::vector<NAM_SAMPLE> processedBuffer(bufferSize * sfInfo.channels);

  sf_count_t numFrames = ((endFrame - startFrame) / bufferSize);

  sf_count_t numBuffers = ((endFrame - startFrame) / bufferSize) + 1;
  sf_count_t restBuffer = ((endFrame - startFrame) % bufferSize) > 0 ? 1 : 0;

  sf_count_t startPosition = sf_seek(inputFilePtr, startFrame, 0);

  std::cout << "t_id " << threadId << " restBuffer=" << ((endFrame - startFrame) % bufferSize) << std::endl;
  std::cout << "t_id " << threadId << " numFrames=" << numFrames << std::endl;
  std::cout << "t_id " << threadId << " startPosition=" << startPosition << std::endl;
  std::cout << "t_id " << threadId << " endFrame=" << endFrame << std::endl;

  for (sf_count_t frameIndex = 0; frameIndex < numBuffers + restBuffer; ++frameIndex)
  {
    sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);

    std::cout << "t_id " << threadId << " frame " << frameIndex + 1 << "/" << numFrames << std::endl;

    if (bytesRead <= 0)
    {
      std::cerr << "End of file or error" << std::endl;
      // End of file or error
      break;
    }

    if (processedBuffer.size() != bytesRead)
    {
      processedBuffer.resize(bytesRead);
      buffer.resize(bytesRead);
    };

    std::cout << "t_id " << threadId << " bytesRead = " << bytesRead << std::endl;

    model->process(buffer.data(), processedBuffer.data(), bytesRead);
    model->finalize_(bytesRead);

    // std::cout << "processedBuffer.size() = " << processedBuffer.size() << std::endl;
    // std::cout << "buffer.size() = " << buffer.size() << std::endl;

    // std::memcpy(result.data(), processedBuffer.data(), bytesRead);
    // std::copy(processedBuffer.begin(), processedBuffer.begin() + bytesRead, result.end());
    // result.insert(result.end(), processedBuffer.begin(), processedBuffer.end());

    // Loop to append data from vector2 to vector1
    {
      // std::lock_guard<std::mutex> lock(bufferMutex);
      for (const auto& element : processedBuffer)
      {
        result.insert(result.end(), element);
        // result.push_back(element); // or vector1.insert(vector1.end(), element);
      }
    }
  }

  sf_close(inputFilePtr);

  // Lock the mutex before modifying the shared result vector
  // std::lock_guard<std::mutex> lock(mutex);

  std::cout << "Thread ended id " << threadId << std::endl;
}

int main(int argc, char* argv[])
{
  const int bufferSize = 8192;

  // Turn on fast tanh approximation
  nam::activations::Activation::enable_fast_tanh();

  checkInputParameters(argc, argv);

  char* modelFileName = argv[1];
  char* inputFileName = argv[2];

  std::unique_ptr<nam::DSP> model = loadModel(modelFileName);

  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);

  SF_INFO outputInfo = sfInfo; // Copy input file info
  SNDFILE* outputFilePtr = loadSoundfile(argv[3], SFM_WRITE, &outputInfo);

  if (!inputFilePtr || !outputFilePtr)
  {
    sf_close(inputFilePtr);
    sf_close(outputFilePtr);
    exit(1);
  }

  const int numThreads = 10; // Set your desired number of threads

  std::vector<NAM_SAMPLE> buffer(bufferSize * sfInfo.channels);
  std::vector<NAM_SAMPLE> processedBuffer; //(bufferSize * sfInfo.channels);
  // std::vector<std::vector<NAM_SAMPLE>> resultVectors(numThreads, std::vector<NAM_SAMPLE>(bufferSize *
  // sfInfo.channels));
  std::vector<std::vector<NAM_SAMPLE>> resultVectors(numThreads, std::vector<NAM_SAMPLE>());

  /**
   * Multithread testing börjar här..
   */


  std::vector<std::thread> threads;
  const unsigned int totalFrames = sfInfo.frames; // 66536; // sfInfo.frames;
  // const unsigned int chunkSize = sfInfo.frames / numThreads;
  const unsigned int bufferBlocks = totalFrames / bufferSize;
  const unsigned int bufferRest = totalFrames % bufferSize;

  // 220500 frames / 8191 buffersize = 26,91650390625
  const int numChunks = totalFrames / bufferSize;
  // 26 / 2 = 13
  const unsigned int chunkSizePerThread = (numChunks / numThreads);

  std::mutex resultMutex;

  std::cout << "totalFrames=" << totalFrames << std::endl;
  std::cout << "chunkSize per thread=" << chunkSizePerThread << std::endl;

  // Launch threads
  for (int i = 0; i < numThreads; ++i)
  // for (auto& vector : resultVectors)
  {

    // sf_count_t startFrame = (i * (chunkSizePerThread + 1)) * bufferSize;
    // sf_count_t endFrame = (i == numThreads - 1) ? totalFrames : startFrame + ((chunkSizePerThread + 1) * bufferSize);

    sf_count_t startFrame;
    sf_count_t endFrame;

    // first fram
    if (i == 0)
    {
      startFrame = 0;
      endFrame = chunkSizePerThread * bufferSize;
    }

    // last frame
    else if (i == numThreads - 1)
    {
      startFrame = ((chunkSizePerThread + 1) * i) * bufferSize;
      endFrame = totalFrames;
    }
    else
    {
      startFrame = ((chunkSizePerThread + 1) * i) * bufferSize;
      endFrame = startFrame + (chunkSizePerThread * bufferSize);
    }
    // std::streampos startFrame = (i == 0 && i == numThreads - 1) ? 0 : (i + 1) *
    // ((chunkSizePerThread)*bufferSize); std::streampos endFrame = (i == numThreads - 1) ? totalFrames : (i +
    // 1) * ((chunkSizePerThread)*bufferSize);

    // std::streampos startFrame = (i * chunkSize);
    // std::streampos endFrame = (i == numThreads - 1) ? sfInfo.frames : (i + 1) * chunkSize;

    // doWork(modelFileName, inputFileName, startFrame, endFrame, i);

    // std::streampos start = i * chunkSize;
    // std::streampos end = (i == numThreads - 1) ? fileSize : (i + 1) * chunkSize;


    threads.emplace_back(doWork, modelFileName, inputFileName, outputFilePtr, startFrame, endFrame, i,
                         std::ref(resultVectors[i]), std::ref(resultMutex), bufferSize);
    // threads.emplace_back(
    //   processThread, std::ref(inputFilename), chunkSize, start, end, std::ref(result), std::ref(resultMutex));
  }

  // Join threads
  for (auto& thread : threads)
  {
    thread.join();
  }

  // std::cout << "processedBuffer.size()=" << processedBuffer.size() << std::endl;
  // std::cout << "sfInfo.channels=" << sfInfo.channels << std::endl;
  // std::cout << "processedBuffer.size() / sfInfo.channels=" << processedBuffer.size() / sfInfo.channels << std::endl;

  // sf_writef_double(outputFilePtr, processedBuffer.data(), processedBuffer.size());

  // sf_writef_double(outputFilePtr, resultVectors[0].data(), resultVectors[0].size());


  // for (auto& vector : resultVectors)
  // {
  //   sf_writef_double(outputFilePtr, vector.data(), vector.size());
  // }


  for (int i = 0; i < numThreads; ++i)
  {
    sf_writef_double(outputFilePtr, resultVectors[i].data(), resultVectors[i].size());
  }

  // sf_count_t numChunks = sfInfo.frames / bufferSize;
  // for (sf_count_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
  // {
  //   sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);

  //   if (bytesRead <= 0)
  //   {
  //     // End of file or error
  //     break;
  //   }

  //   model->process(buffer.data(), processedBuffer.data(), bytesRead);
  //   model->finalize_(bytesRead);

  //   printProgressBar(chunkIndex, numChunks);

  //   sf_writef_double(outputFilePtr, processedBuffer.data(), bytesRead);
  // }

  // Just make the progress bar show 100%
  printProgressBar(100, 100);

  // Close the input and output files
  sf_close(inputFilePtr);
  sf_close(outputFilePtr);

  std::cout << "Audio file successfully processed and written." << std::endl;

  exit(0);
}
