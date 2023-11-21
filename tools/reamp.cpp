#ifdef __APPLE__
  #include <cstdlib> // For macOS and other Apple platforms
#else
  #include <malloc.h> // For other platforms
#endif

#include <iostream>
#include <vector>
#include <fstream>

#include <sndfile.h>

#include "NAM/wav.h"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"

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

int main(int argc, char* argv[])
{

  const int bufferSize = 8096;

  // Turn on fast tanh approximation
  activations::Activation::enable_fast_tanh();

  // Check if the correct number of command-line arguments is provided
  if (argc != 4)
  {
    std::cerr << "Usage: " << argv[0] << " <model_filename> <input_filename> <output_filename>" << std::endl;
    return 1;
  }

  const char* modelPath = argv[1];
  std::cout << "Loading model " << modelPath << "\n";
  std::unique_ptr<DSP> model;  
  model.reset();
  model = std::move(get_dsp(modelPath));

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

  for (sf_count_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
  {
    sf_count_t bytesRead = sf_readf_double(inputFilePtr, buffer.data(), bufferSize);


    if (bytesRead <= 0)
    {
      // End of file or error
      break;
    }

    model->process(buffer.data(), processedBuffer.data(), bytesRead);
    model->finalize_(bytesRead);

    printProgressBar(chunkIndex, numChunks);

    sf_writef_double(outputFilePtr, processedBuffer.data(), bytesRead);
  }

  // Just make the progress bar show 100%
  printProgressBar(100, 100);

  // Close the input and output files
  sf_close(inputFilePtr);
  sf_close(outputFilePtr);

  std::cout << "WAV file successfully processed and written." << std::endl;

  exit(0);
}
