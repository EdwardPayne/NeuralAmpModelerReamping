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

  // Öppna filen
  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = loadSoundfile(inputFileName, SFM_READ, &sfInfo);

  sf_count_t numSamples = (endSample - startSample);
  sf_count_t numBuffers = (numSamples / bufferSize) + 1;
  sf_count_t restBuffer = (numSamples % bufferSize) > 0 ? 1 : 0;

  std::vector<NAM_SAMPLE> buffer(bufferSize);
  std::vector<NAM_SAMPLE> processedBuffer(bufferSize);

  std::cout << "t_id " << threadId << " processedBuffer.size() = " << processedBuffer.size() << std::endl;
  std::cout << "t_id " << threadId << " buffer.size() = " << buffer.size() << std::endl;

  {
    std::lock_guard<std::mutex> lock(outputMutex);
    // std::cout << "Thread start id " << threadId << " startFrame=" << startFrame << " endFrame=" << endFrame
    //           << std::endl;
    std::cout << "Thread start id " << threadId << " numBuffers " << numBuffers << std::endl;
  }

  sf_count_t startPosition = sf_seek(inputFilePtr, startSample, 0);

  // std::cout << "t_id " << threadId << " restBuffer=" << (numSamples % bufferSize) << std::endl;
  // std::cout << "t_id " << threadId << " numBuffers=" << numBuffers << std::endl;
  // std::cout << "t_id " << threadId << " startPosition=" << startPosition << std::endl;
  // std::cout << "t_id " << threadId << " startFrame=" << startSample << std::endl;
  // std::cout << "t_id " << threadId << " endFrame=" << endSample << std::endl;

  for (sf_count_t bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
  // for (sf_count_t frameIndex = startFrame; frameIndex <= endFrame; frameIndex += bufferSize)
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
    //   std::cout << "Adjusting buffer size to " << bytesRead << std::endl;
    //   processedBuffer.resize(bytesRead);
    //   processedBuffer.reserve(bytesRead);
    //   buffer.resize(bytesRead);
    // };


    // model->process(buffer.data(), processedBuffer.data(), bytesRead);
    // model->finalize_(bytesRead);


    // std::cout << "t_id " << threadId << " frameIndex = " << bufferIndex << std::endl;
    // std::cout << "t_id " << threadId << " result.size() = " << result.size() << std::endl;
    // std::cout << "t_id " << threadId << " bytesRead = " << bytesRead << std::endl;
    // std::cout << "t_id " << threadId << " processedBuffer.size() = " << processedBuffer.size() << std::endl;
    // std::cout << "t_id " << threadId << " buffer.size() = " << buffer.size() << std::endl;

    // Loop to append data from processedBuffer to result
    for (const auto& element : buffer)
    {
      result.insert(result.end(), element);
    }
  }

  sf_close(inputFilePtr);

  // std::cout << "Thread ended id " << threadId << std::endl;
}


void loopAudio(const char* inputFilename, const char* outputFilename)
{
  SF_INFO sfInfo;
  SNDFILE* inputFile = sf_open(inputFilename, SFM_READ, &sfInfo);
  if (!inputFile)
  {
    std::cerr << "Error opening input file: " << sf_strerror(nullptr) << std::endl;
    return;
  }

  // Check if the file is stereo
  if (sfInfo.channels != 1 && sfInfo.channels != 2)
  {
    std::cerr << "Unsupported number of channels. Only mono and stereo files are supported." << std::endl;
    sf_close(inputFile);
    return;
  }

  // Set up buffer for reading audio data
  const int buffer_size = 1024;
  float buffer[buffer_size * sfInfo.channels];

  SF_INFO outputInfo = sfInfo;
  SNDFILE* outputFile = sf_open(outputFilename, SFM_WRITE, &outputInfo);
  if (!outputFile)
  {
    std::cerr << "Error opening output file: " << sf_strerror(nullptr) << std::endl;
    sf_close(inputFile);
    return;
  }

  while (true)
  {
    sf_count_t num_frames = sf_readf_float(inputFile, buffer, buffer_size);
    if (num_frames <= 0)
    {
      break;
    }

    // Process the audio data here
    // ...

    // You can access individual samples in the buffer like this:
    // buffer[i] for mono, buffer[i * 2] and buffer[i * 2 + 1] for stereo

    // Write the processed frames to the output file
    sf_writef_float(outputFile, buffer, num_frames);

    // Print some information
    std::cout << "Read " << num_frames << " frames." << std::endl;

    // If you want to exit the loop after processing a certain number of frames, you can add a condition here.
    // For example, if you want to process only the first 1000 frames:
    // if (num_frames >= 1000) break;
  }

  // Close the files when done
  sf_close(inputFile);
  sf_close(outputFile);
}

int main(int argc, char* argv[])
{
  int bufferSize = 1024;

  // Turn on fast tanh approximation
  nam::activations::Activation::enable_fast_tanh();

  checkInputParameters(argc, argv);

  char* modelFileName = argv[1];
  char* inputFileName = argv[2];
  char* outputFileName = argv[3];
  int threadsOverride = (argc == 5 && atoi(argv[4]) > 0) ? atoi(argv[4]) : 0;

  // loopAudio(inputFileName, outputFileName);
  //   return 1;

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

  const unsigned int totalFrames = sfInfo.frames; // 66536; // sfInfo.frames;
  const sf_count_t totalSamples = sfInfo.frames * sfInfo.channels;

  // 220500 frames / 8192 buffersize = 26,91650390625
  // const int numChunks = totalFrames / bufferSize;
  const int numChunks = totalSamples / bufferSize;
  const unsigned int numThreadsMax = threadsOverride > 0 ? threadsOverride : std::thread::hardware_concurrency();
  unsigned int numThreads = numThreadsMax;

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

  // results from all threads
  std::vector<std::vector<NAM_SAMPLE>> resultVectors(numThreads, std::vector<NAM_SAMPLE>());

  const unsigned int chunkSizePerThread = (numChunks / numThreads);

  // std::cout << "threads=" << numThreads << std::endl;
  // std::cout << "totalFrames=" << totalFrames << std::endl;
  // std::cout << "numChunks=" << numChunks << std::endl;
  // std::cout << "totalSamples=" << totalSamples << std::endl;
  // std::cout << "channels=" << sfInfo.channels << std::endl;
  // std::cout << "chunkSize per thread=" << chunkSizePerThread << std::endl;

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

  // Just make the progress bar show 100%
  // printProgressBar(100, 100);

  std::cout << "Audio file successfully processed and written." << std::endl;

  // Close the input and output files
  sf_close(inputFilePtr);
  sf_close(outputFilePtr);


  exit(0);
}
