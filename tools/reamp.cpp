#include "malloc.h"
#include <iostream>
#include <fstream>
#include <chrono>

#include "NAM/wav.h"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

#define AUDIO_BUFFER_SIZE 64

double buffer[AUDIO_BUFFER_SIZE];


class WavHeader
{
public:
  char riff[4];
  uint32_t fileSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
};

void processAudioBuffer(std::unique_ptr<DSP>& model, NAM_SAMPLE* input, NAM_SAMPLE* output, int numFrames)
{
  // Assuming the DSP::process function takes input, output, and num_frames
  model->process(input, output, numFrames);
  model->finalize_(numFrames);
}

// Read audio data from a WAV file
bool readWavFile(const std::string& filename, double*& audioData, WavHeader& header)
{
  std::ifstream file(filename, std::ios::binary);

  if (!file.is_open())
  {
    std::cerr << "Error opening file: " << filename << std::endl;
    return false;
  }

  file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

  // Check if the file is a WAV file
  if (std::string(header.riff, 4) != "RIFF" || std::string(header.wave, 4) != "WAVE")
  {
    std::cerr << "Not a valid WAV file: " << filename << std::endl;
    return false;
  }

  // Read audio data
  const size_t dataSize = header.dataSize / sizeof(double);
  audioData = new double[dataSize];
  file.read(reinterpret_cast<char*>(audioData), header.dataSize);

  file.close();

  return true;
}

// Write audio data to a WAV file
bool writeWavFile(const std::string& filename, const double* audioData, const WavHeader& header)
{
  std::ofstream file(filename, std::ios::binary);

  if (!file.is_open())
  {
    std::cerr << "Error opening file for writing: " << filename << std::endl;
    return false;
  }

  // Write WAV header
  file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));

  // Write audio data
  file.write(reinterpret_cast<const char*>(audioData), header.dataSize);

  file.close();

  return true;
}

int main(int argc, char* argv[])
{

  // Check if the correct number of command-line arguments is provided
  if (argc != 4)
  {
    std::cerr << "Usage: " << argv[0] << " <model_filename> <input_filename> <output_filename>" << std::endl;
    return 1;
  }

  const char* modelPath = argv[1];
  std::cout << "Loading model " << modelPath << "\n";

  // Turn on fast tanh approximation
  activations::Activation::enable_fast_tanh();

  std::unique_ptr<DSP> model;
  model.reset();
  model = std::move(get_dsp(modelPath));

  if (model == nullptr)
  {
    std::cerr << "Failed to load model\n";
    exit(1);
  }

  // Process the audio data using DSP in chunks
  const int bufferSize = 8096;
  double sampleRate = 44100;

  const char* inputFilename = argv[2];
  const char* outputFilename = argv[3];

  std::ofstream outputFile(outputFilename, std::ios::binary);

  // Load audio data from WAV file
  std::vector<float> inputBuffer;
  dsp::wav::Load(inputFilename, inputBuffer, sampleRate);

  // Cast inputBuffer to vector of doubles
  std::vector<double> doubleInputBuffer(inputBuffer.begin(), inputBuffer.end());


  // Process the audio data in chunks
  const int numFrames = doubleInputBuffer.size();
  std::vector<NAM_SAMPLE> outputBuffer(numFrames);

  // std::cout << "datasize " << inputHeader.dataSize << std::endl;
  std::cout << "inputBuffer " << inputBuffer.size() << std::endl;
  std::cout << "outputBuffer " << sizeof(outputBuffer) << std::endl;
  std::cout << "size of double " << sizeof(double) << std::endl;

  std::vector<double> doubleBuffer(inputBuffer.begin(), inputBuffer.end());

  for (int i = 0; i < numFrames; i += bufferSize)
  {
    int chunkSize = std::min(bufferSize, numFrames - i);

    std::cout << "i=" << i << " chunkSize=" << chunkSize << std::endl;

    // Call the process function for each chunk
    model->process(&doubleInputBuffer[i], &outputBuffer[i], chunkSize);
    model->finalize_(bufferSize);

    // Write the processed chunk to the output file
    outputFile.write(reinterpret_cast<const char*>(&outputBuffer[i]), chunkSize * sizeof(NAM_SAMPLE));
  }

  outputFile.close();

  std::cout << "WAV file successfully processed and written." << std::endl;

  exit(0);
}
