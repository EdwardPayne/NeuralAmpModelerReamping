#include <iostream>
#include <fstream>
#include <memory>

// Replace this include with the actual header file for your DSP code
#include "DSP.h"

class WavHeader {
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

// Replace this function with your DSP processing code
void processAudioBuffer(std::unique_ptr<DSP>& model, double* input, double* output, int numFrames) {
    // Assuming the DSP::process function takes input, output, and num_frames
    model->process(input, output, numFrames);
}

// Read audio data from a WAV file
bool readWavFile(const std::string& filename, double*& audioData, WavHeader& header) {
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    // Check if the file is a WAV file
    if (std::string(header.riff, 4) != "RIFF" || std::string(header.wave, 4) != "WAVE") {
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
bool writeWavFile(const std::string& filename, const double* audioData, const WavHeader& header) {
    std::ofstream file(filename, std::ios::binary);

    if (!file.is_open()) {
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

int main(int argc, char* argv[]) {
    // Check if the correct number of command-line arguments is provided
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_filename.wav> <output_filename.wav>" << std::endl;
        return 1;
    }

    // Read input audio file
    const char* inputFilename = argv[1];
    double* inputBuffer = nullptr;
    WavHeader inputHeader;

    if (!readWavFile(inputFilename, inputBuffer, inputHeader)) {
        return 1;
    }

    // Process the audio data using DSP in chunks
    const size_t bufferSize = 1024;
    std::unique_ptr<DSP> model = std::make_unique<DSP>();

    // Allocate memory for the output buffer
    double* outputBuffer = new double[inputHeader.dataSize / sizeof(double)];

    for (size_t i = 0; i < inputHeader.dataSize / sizeof(double); i += bufferSize) {
        size_t chunkSize = std::min(bufferSize, inputHeader.dataSize / sizeof(double) - i);

        // Process the current chunk
        processAudioBuffer(model, inputBuffer + i, outputBuffer + i, chunkSize);
    }

    // Write the processed data to the output audio file
    const char* outputFilename = argv[2];
    WavHeader outputHeader = inputHeader;
    outputHeader.dataSize = static_cast<uint32_t>(inputHeader.dataSize);

    if (!writeWavFile(outputFilename, outputBuffer, outputHeader)) {
        return 1;
    }

    // Clean up allocated memory
    delete[] inputBuffer;
    delete[] outputBuffer;

    std::cout << "WAV file successfully processed and written." << std::endl;

    return 0;
}
