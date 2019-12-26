
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iterator>
#include <vector>
#include <map>

constexpr uint32_t VERSION_NUMBER = 1;

struct Vec2 {
    float s;
    float t;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float s;
    float t;
    static Vertex from(Vec3 position, Vec2 texel, Vec3 normal);
};

struct FaceIndices {
    uint16_t i0;
    uint16_t i1;
    uint16_t i2;
};

struct Grouping {
    uint32_t positionIndex;
    uint32_t texelIndex;
    uint32_t normalIndex;
};

struct Model {
    std::string name;
    static std::vector<Vec3> rawPositions;
    static std::vector<Vec2> rawTexels;
    static std::vector<Vec3> rawNormals;
    std::vector<Vertex> interleavedVertices;
    std::vector<FaceIndices> faceIndices;
    uint16_t getIndex(uint64_t indexPosition, uint64_t indexTexel, uint64_t indexNormal, const Vertex& vertex);
private:
    std::map<uint64_t, uint16_t> indexMap;
};

struct ModelFactory {
    ModelFactory(int argc, char* argv[]);
    void extractAllModelsFromFile();
    void exportAllModels();
    std::string getStatus();
    bool isUsable;
    std::string lastError;
private:
    bool extractNextModelFromStream(std::ifstream& fileInputStream);
    void writeModelFile(const std::string& outputFileName, const Model& model);
    std::vector<Model> models;
    std::string fileName;
    bool includeNormals;
    bool includeTexCoords;
};

std::vector<Vec3> Model::rawPositions;
std::vector<Vec2> Model::rawTexels;
std::vector<Vec3> Model::rawNormals;

Vertex Vertex::from(Vec3 position, Vec2 texel, Vec3 normal) {
    return Vertex{ position.x, position.y, position.z, normal.x, normal.y, normal.z, texel.s, texel.t };
}

uint16_t Model::getIndex(uint64_t indexPosition, uint64_t indexTexel, uint64_t indexNormal, const Vertex& vertex) {
    uint64_t identifier = indexNormal + (indexTexel << 16) + (indexPosition << 32);
    auto position = indexMap.find(identifier);
    if (position == indexMap.end()) {
        uint16_t newIndex = interleavedVertices.size();
        indexMap.emplace(std::pair<uint64_t,uint32_t>(identifier, newIndex));
        interleavedVertices.emplace_back(vertex);
        return newIndex;
    } else {
        return (*position).second;
    }
}

ModelFactory::ModelFactory(int argc, char* argv[]) : fileName() {
    // Set default state
    isUsable = false;
    includeNormals = false;
    includeTexCoords = false;

    // Parse arguments
    for (int arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "--normals") == 0 || strcmp(argv[arg], "-n") == 0) {
            includeNormals = true;
        } else if (strcmp(argv[arg], "--texcoords") == 0 || strcmp(argv[arg], "-t") == 0) {
            includeTexCoords = true;
        } else {
            if (!fileName.empty()) {
                lastError = "Unknown argument: " + std::string(argv[arg]);
                return;
            } else {
                fileName = std::string(argv[arg]);
            }
        }
    }

    // Flag usability
    if (fileName.empty()) {
        lastError = "No file supplied";
    } else {
        isUsable = true;
    }
}

std::string ModelFactory::getStatus() {
    if (includeNormals && includeTexCoords) {
        return "Including normals and texture coordinates";
    } else if (includeNormals) {
        return "Including normals";
    } else if (includeTexCoords) {
        return "Including texture coordinates";
    } else {
        return "Including position data only (no flags were supplied)";
    }
}

bool ModelFactory::extractNextModelFromStream(std::ifstream& fileInputStream) {
    std::string keyObject = "o";
    std::string keyVertex = "v";
    std::string keyTexel = "vt";
    std::string keyNormal = "vn";
    std::string keyFace = "f";

    // Consume name
    Model model;
    fileInputStream >> model.name;

    // Read vertixes, texels, normals and the next o identifier
    while (!fileInputStream.eof()) {
        std::string type;
        fileInputStream >> type;
        if (type == keyVertex) {
            float x, y, z;
            fileInputStream >> x;
            fileInputStream >> y;
            fileInputStream >> z;
            Model::rawPositions.emplace_back(Vec3{ x, y, z });
        }
        else if (type == keyTexel) {
            float s, t;
            fileInputStream >> s;
            fileInputStream >> t;
            Model::rawTexels.emplace_back(Vec2{ s, t });
        }
        else if (type == keyNormal) {
            float nx, ny, nz;
            fileInputStream >> nx;
            fileInputStream >> ny;
            fileInputStream >> nz;
            Model::rawNormals.emplace_back(Vec3{ nx, ny, nz });
        }
        else if (type == keyFace) {
            std::vector<Grouping> faceGroupings;
            std::string line;
            std::getline(fileInputStream, line);
            std::istringstream lineStream(line);
            while (!lineStream.eof()) {
                std::string grouping;
                lineStream >> grouping;
                size_t firstSlash = grouping.find('/');
                size_t secondSlash = grouping.rfind('/');
                unsigned int positionIndex = std::atoi(grouping.substr(0, firstSlash).c_str()) - 1;
                unsigned int texelIndex = std::atoi(grouping.substr(firstSlash + 1, secondSlash - firstSlash - 1).c_str()) - 1;
                unsigned int normalIndex = std::atoi(grouping.substr(secondSlash + 1).c_str()) - 1;
                faceGroupings.emplace_back(Grouping{ positionIndex, texelIndex, normalIndex });
            }

            uint16_t startIndex;
            {
                const auto& grouping = faceGroupings[0];
                const auto& position = Model::rawPositions[grouping.positionIndex];
                const auto& texel = Model::rawTexels[grouping.texelIndex];
                const auto& normal = Model::rawNormals[grouping.normalIndex];
                const Vertex vertex = Vertex::from(Vec3{ position.x, position.y, position.z }, Vec2{ texel.s, texel.t }, Vec3{ normal.x, normal.y, normal.z });
                startIndex = model.getIndex(grouping.positionIndex, grouping.texelIndex, grouping.normalIndex, vertex);
            }

            uint16_t secondPolyIndex;
            {
                const auto& grouping = faceGroupings[1];
                const auto& position = Model::rawPositions[grouping.positionIndex];
                const auto& texel = Model::rawTexels[grouping.texelIndex];
                const auto& normal = Model::rawNormals[grouping.normalIndex];
                const auto& vertex = Vertex::from(Vec3{ position.x, position.y, position.z }, Vec2{ texel.s, texel.t }, Vec3{ normal.x, normal.y, normal.z });
                secondPolyIndex = model.getIndex(grouping.positionIndex, grouping.texelIndex, grouping.normalIndex, vertex);
            }

            size_t noOfGroupings = faceGroupings.size();
            for (size_t i = 2; i < noOfGroupings; i++) {
                const auto& grouping = faceGroupings[i];
                const auto& position = Model::rawPositions[grouping.positionIndex];
                const auto& texel = Model::rawTexels[grouping.texelIndex];
                const auto& normal = Model::rawNormals[grouping.normalIndex];
                const auto& vertex = Vertex::from(Vec3{ position.x, position.y, position.z }, Vec2{ texel.s, texel.t }, Vec3{ normal.x, normal.y, normal.z });
                uint16_t thirdPolyIndex = model.getIndex(grouping.positionIndex, grouping.texelIndex, grouping.normalIndex, vertex);
                model.faceIndices.emplace_back(FaceIndices{ startIndex, secondPolyIndex, thirdPolyIndex });
                secondPolyIndex = thirdPolyIndex;
            }
        }
        else if (type == keyObject) {
            models.emplace_back(model);
            return true;
        }
    }
    models.emplace_back(model);
    return false;
}

void ModelFactory::extractAllModelsFromFile() {
    std::ifstream fileInputStream;
    fileInputStream.open(fileName);
    if (!fileInputStream.good()) {
        std::cout << "Error opening file " << fileName << std::endl;
        exit(1);
    }

    std::string keyObject = "o";
    std::string keyFace = "f";
    while (!fileInputStream.eof()) {
        std::string type;
        fileInputStream >> type;
        if (type == keyObject) {
            while (extractNextModelFromStream(fileInputStream));
            break;
        }
    }

    fileInputStream.close();
}

void ModelFactory::writeModelFile(const std::string& outputFileName, const Model& model) {
    std::ofstream fileStream;
    fileStream.open(outputFileName, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!fileStream.good()) {
        std::cout << "Error creating file " << outputFileName << std::endl;
        exit(1);
    }

    // Write version code and data structure flags
    auto normalFlag = (uint32_t)includeNormals;
    auto texCoordFlag = (uint32_t)includeTexCoords;
    fileStream.write((const char*)&VERSION_NUMBER, sizeof(uint32_t));
    fileStream.write((const char*)&normalFlag, sizeof(uint32_t));
    fileStream.write((const char*)&texCoordFlag, sizeof(uint32_t));

    // Write interleaved vertex data
    uint32_t vertexCount = model.interleavedVertices.size();
    fileStream.write((const char*)&vertexCount, sizeof(uint32_t));
    if (includeNormals && includeTexCoords) {
        for (const auto &vertex : model.interleavedVertices) {
            fileStream.write((const char*)&vertex, sizeof(Vertex));
        }
    } else if (includeNormals) {
        for (const auto &vertex : model.interleavedVertices) {
            fileStream.write((const char*)&vertex, 6 * sizeof(float));
        }
    } else if (includeTexCoords) {
        for (const auto &vertex : model.interleavedVertices) {
            fileStream.write((const char*)&vertex, 3 * sizeof(float));
            fileStream.write((const char*)&vertex + (6 * sizeof(float)), 2 * sizeof(float));
        }
    } else {
        for (const auto &vertex : model.interleavedVertices) {
            fileStream.write((const char*)&vertex, 3 * sizeof(float));
        }
    }

    // Write indices
    uint32_t indexCount = model.faceIndices.size();
    fileStream.write((const char*)&indexCount, sizeof(uint32_t));
    for (const auto& faceIndices : model.faceIndices) {
        fileStream.write((const char*)&faceIndices, sizeof(FaceIndices));
    }

    // Finish
    fileStream.close();
}

void ModelFactory::exportAllModels() {
    std::cout << "Files written:";
    for (const auto& model : models) {
        std::string outputFileName = model.name + ".mdl";
        writeModelFile(outputFileName, model);
        std::cout << " " << outputFileName;
    }
}

int main(int argc, char* argv[]) {
    // Check file name was provided
    if (argc < 2) {
        std::string fullCommand(argv[0]);
        size_t lastPos = fullCommand.find_last_of('\\');
        std::string commandWithoutPath = lastPos == std::string::npos ? fullCommand : fullCommand.substr(lastPos + 1);
        std::cerr << "Usage: " << commandWithoutPath << " OBJ_FILE_NAME [FLAGS]" << std::endl;
        exit(1);
    }

    // Check for request of help
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        std::string fullCommand(argv[0]);
        size_t lastPos = fullCommand.find_last_of('\\');
        std::string commandWithoutPath = lastPos == std::string::npos ? fullCommand : fullCommand.substr(lastPos + 1);
        std::cout << "Usage: " << commandWithoutPath << " OBJ_FILE_NAME [FLAGS]" << std::endl;
        std::cout << "Flags:" << std::endl;
        std::cout << "  --help            Print this usage information" << std::endl;
        std::cout << "  --normals, -n     Include normals in the exported file" << std::endl;
        std::cout << "  --texcoords, -t   Include texture coordinates in the exported file" << std::endl;
        exit(1);
    }

    // Decode model from input file
    ModelFactory factory(argc, argv);
    if (!factory.isUsable) {
        std::cerr << factory.lastError << std::endl;
        exit(1);
    }
    std::cout << factory.getStatus() << std::endl;
    factory.extractAllModelsFromFile();

    // Output each model to a file
    factory.exportAllModels();
    exit(1);
}
