#ifdef __APPLE__
  #include <cstdlib> // For macOS and other Apple platforms
#else
  #include <malloc.h> // For other platforms
#endif

#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <sndfile.h>

#include "NAM/wav.h"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"

std::mutex ioMutex;
std::atomic<int> chunksProcessed(0);
std::condition_variable cv;
bool allThreadsFinished = false;

/*
void printProgressBarORIGINAL(int current, int total, int width = 40)
{
  float progress = static_cast<float>(current) / total;
  int barWidth = static_cast<int>(progress * width);

  std::lock_guard<std::mutex> lock(ioMutex);
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

void processAudioChunkOLD(std::unique_ptr<nam::DSP>& model, std::vector<NAM_SAMPLE>& buffer,
                       std::vector<NAM_SAMPLE>& processedBuffer, sf_count_t chunkIndex, sf_count_t numChunks,
                       SNDFILE* inputFilePtr, SNDFILE* outputFilePtr, int bufferSize)
{

  sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);
  std::cout << "processAudioChunk " << bytesRead << std::endl;

  if (bytesRead <= 0)
  {
    // End of file or error
    return;
  }

  model->process(buffer.data(), processedBuffer.data(), bytesRead);
  model->finalize_(bytesRead);

  // {
  //   std::lock_guard<std::mutex> lock(ioMutex);
  //   ++chunksProcessed;
  //   // printProgressBar(chunksProcessed, numChunks);
  // }
  cv.notify_one(); // Notify waiting threads that a chunk has been processed

  sf_writef_double(outputFilePtr, processedBuffer.data(), bytesRead);
}
*/

void printProgressBar(int current, int total, int width = 40)
{
  std::ostringstream progressBar;

  {
    std::lock_guard<std::mutex> lock(ioMutex);

    float progress = static_cast<float>(current) / total;
    int barWidth = static_cast<int>(progress * width);

    progressBar << "[";
    for (int i = 0; i < width; ++i)
    {
      if (i < barWidth)
      {
        progressBar << "=";
      }
      else
      {
        progressBar << " ";
      }
    }

    progressBar << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%\r";
  }

  std::cout << progressBar.str();
  std::cout.flush();

  // Print a newline when the progress is complete
  if (current == total)
  {
    std::cout << std::endl;
  }
}


void processAudioChunk(std::unique_ptr<nam::DSP>& model, std::vector<NAM_SAMPLE>& buffer,
                       std::vector<NAM_SAMPLE>& processedBuffer, sf_count_t chunkIndex, sf_count_t numChunks,
                       SNDFILE* inputFilePtr, SNDFILE* outputFilePtr, int bufferSize)
{
  sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);
  std::cout << "processAudioChunk " << bytesRead << std::endl;

  if (bytesRead <= 0)
  {
    // End of file or error
    return;
  }

  model->process(buffer.data(), processedBuffer.data(), bytesRead);
  model->finalize_(bytesRead);

  // {
  //   std::lock_guard<std::mutex> lock(ioMutex);
  //   ++chunksProcessed;
  //   //   // printProgressBar(chunksProcessed, numChunks);
  //   cv.notify_one(); // Notify waiting threads that a chunk has been processed
  // }

  sf_writef_double(outputFilePtr, processedBuffer.data(), bytesRead);
}

int main(int argc, char* argv[])
{
  const int bufferSize = 8096;

  // Turn on fast tanh approximation
  nam::activations::Activation::enable_fast_tanh();

  // Check if the correct number of command-line arguments is provided
  if (argc != 4)
  {
    std::cerr << "Usage: " << argv[0] << " <model_filename> <input_filename> <output_filename>" << std::endl;
    return 1;
  }

  const char* modelPath = argv[1];
  std::cout << "Loading model " << modelPath << "\n";
  std::unique_ptr<nam::DSP> model;
  model.reset();
  model = std::move(nam::get_dsp(modelPath));

  if (model == nullptr)
  {
    std::cerr << "Failed to load model\n";
    exit(1);
  }

  const char* inputFilename = argv[2];
  const char* outputFilename = argv[3];

  // Open the input WAV file
  SF_INFO sfInfo;
  SNDFILE* inputFilePtr = sf_open(inputFilename, SFM_READ, &sfInfo);
  if (!inputFilePtr)
  {
    std::cerr << "Error opening input file: " << sf_strerror(NULL) << std::endl;
    return 1;
  }

  // Open the output WAV file for writing
  SF_INFO outputInfo = sfInfo; // Copy input file info
  SNDFILE* outputFilePtr = sf_open(outputFilename, SFM_WRITE, &outputInfo);
  if (!outputFilePtr)
  {
    std::cerr << "Error opening output file: " << sf_strerror(NULL) << std::endl;
    sf_close(inputFilePtr);
    return 1;
  }

  std::vector<NAM_SAMPLE> buffer(bufferSize * sfInfo.channels);
  std::vector<NAM_SAMPLE> processedBuffer(bufferSize * sfInfo.channels);

  sf_count_t numChunks = sfInfo.frames / bufferSize;

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

  // // Just make the progress bar show 100%
  // printProgressBar(100, 100);


  const unsigned int numCores = std::thread::hardware_concurrency();
  std::cout << "Number of cores: " << numCores << std::endl;
  std::vector<std::thread> threads;

  for (unsigned int i = 0; i < numCores; ++i)
  {
    threads.emplace_back([&]() {
      while (true)
      {
        std::unique_lock<std::mutex> lock(ioMutex);
        cv.wait(lock, [&] { return chunksProcessed < numChunks || allThreadsFinished; });

        sf_count_t chunkIndex = chunksProcessed;
        if (chunkIndex >= numChunks)
        {
          allThreadsFinished = true;
          lock.unlock();
          cv.notify_all();
          break; // All chunks processed
        }

        processAudioChunk(
          model, buffer, processedBuffer, chunkIndex, numChunks, inputFilePtr, outputFilePtr, bufferSize);

        lock.unlock();
      }
    });
  }


  // Wait for all threads to finish
  {
    std::unique_lock<std::mutex> lock(ioMutex);
    cv.wait(lock, [&] {
      std::cout << "Waiting.." << std::endl;
      return allThreadsFinished;
    });
  }

  // Close the input and output files
  sf_close(inputFilePtr);
  sf_close(outputFilePtr);

  std::cout << "WAV file successfully processed and written." << std::endl;

  exit(0);
}
