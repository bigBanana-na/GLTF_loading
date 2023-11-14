#pragma once

#include <fstream>
#include <iostream>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/shader.h>
#include <camera.h>

#include <tiny_gltf.h>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION

#define BUFFER_OFFSET(i) ((char *)NULL + (i))


class GLTF_Model
{
public:
    bool gammaCorrection;
    void draw(Shader& shader);                          //draw the model

    //class constructor
    GLTF_Model(std::string const& path, Camera &camera,bool gamma = false) : gammaCorrection(gamma){
        if (!loadModel(model, path.c_str())) return;
        //define texture unit index
        textureUnitIndices["baseColorTexture"] = 0;
        textureUnitIndices["metallicRoughnessTexture"] = 1;
        textureUnitIndices["normalTexture"] = 2;
        textureUnitIndices["occlusionTexture"] = 3;
        //initialize none-textures mesh factors
        baseColorFactor = glm::vec4(-1.0f, -1.0f, -1.0f, -1.0f);
        metallicFactor = -1.0f;
        roughnessFactor = -1.0f;

        mycamera = camera;
    }
    ~GLTF_Model() {
        for (GLuint i = 0; i < VaosAndEbos.first.size();i++) {
            glDeleteVertexArrays(1, &VaosAndEbos.first[i]);
        }
        // cleanup vbos but do not delete index buffers yet
        for (auto it = VaosAndEbos.second.cbegin(); it != VaosAndEbos.second.cend();) {
            tinygltf::BufferView bufferView = model.bufferViews[it->first];
            if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
                glDeleteBuffers(1, &VaosAndEbos.second[it->first]);
                VaosAndEbos.second.erase(it++);
            }
            else {
                ++it;
            }
        }
    }

private:
    Camera mycamera;
    std::vector<GLuint> textureIDs;; //textures index
    std::map<std::string, int> textureUnitIndices; //textures unit index
    std::pair<std::map<int, GLuint>, std::map<int, GLuint>> VaosAndEbos;//vaos and ebos to bound
    //model to loaded
    tinygltf::Model model;         
    //none-textures mesh factors
    glm::vec4 baseColorFactor;     
    float metallicFactor;
    float roughnessFactor;
    //lights in scene
    struct PointLight {
        glm::vec3 position;  
        glm::vec3 color;    
        float intensity;    
    };
    struct DirectionalLight {
        glm::vec3 direction;  
        glm::vec3 color;     
        float intensity;      
    };
    std::vector<PointLight> pointLights;
    std::vector<DirectionalLight> directionalLights;


    bool loadModel(tinygltf::Model& model, const char* filename);

    void bindMesh(std::map<int, GLuint>& vbos,
        tinygltf::Model& model, tinygltf::Mesh& mesh);
    void bindModelNodes(std::map<int, GLuint>& vaos, std::map<int, GLuint>& vbos, tinygltf::Model& model,
        tinygltf::Node& node);
    std::pair<std::map<int, GLuint>, std::map<int, GLuint>> bindModel(tinygltf::Model& model);
    void drawMesh(const std::map<int, GLuint>& vbos,
        tinygltf::Model& model, tinygltf::Mesh& mesh, Shader& shader);
    void drawModelNodes(const std::pair<std::map<int, GLuint>, std::map<int, GLuint>> VaosAndEbos,
        tinygltf::Model& model, tinygltf::Node& node, Shader& shader);
    void drawModel(const std::pair<std::map<int, GLuint>, std::map<int, GLuint>> VaosAndEbos,
        tinygltf::Model& model, Shader& shader);
    void dbgModel(tinygltf::Model& model);              //debug my class
    void bindVbo(std::map<int, GLuint>& vbos, tinygltf::Model& model, size_t indice);
    glm::mat4 getModelMatrix(tinygltf::Node& node, float angle);
};


bool GLTF_Model::loadModel(tinygltf::Model& model, const char* filename) {
    tinygltf::TinyGLTF loader;
    std::string err_binary;
    std::string warn_binary;
    std::string err_ascii;
    std::string warn_ascii;
    //load binary gltf file first
    bool res = loader.LoadBinaryFromFile(&model, &err_binary, &warn_binary, filename);
    //if binary gltf not exists , load ASCII gltf
    if (!res) {
        res = loader.LoadASCIIFromFile(&model, &err_ascii, &warn_ascii, filename);
    }
    if (!res) {
        std::cout << "Failed to load glTF: " << filename << std::endl;
        if (!warn_binary.empty()) {
            std::cout << "WARN_BINARY: " << warn_binary << std::endl;
        }
        if (!err_binary.empty()) {
            std::cout << "ERR_BINARY: " << err_binary << std::endl;
        }
        if (!err_ascii.empty()) {
            std::cout << "WARN_ASCII: " << err_ascii << std::endl;
        }
        if (!warn_ascii.empty()) {
            std::cout << "ERR_ASCII: " << warn_ascii << std::endl;
        }
    }
    else
        std::cout << "Loaded glTF: " << filename << std::endl;

    //bind model
    VaosAndEbos = bindModel(model);
    //debug model
    //dbgModel(model);
    return res;
}

void GLTF_Model:: bindMesh(std::map<int, GLuint>& vbos,
    tinygltf::Model& model, tinygltf::Mesh& mesh) {

    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
        bindVbo(vbos, model, primitive.indices);
        
        //bind attribute pointer and bind vbo
        for (auto& attrib : primitive.attributes) {
            tinygltf::Accessor accessor = model.accessors[attrib.second];
            bindVbo(vbos, model, attrib.second);
            int byteStride =
                accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

            int size = 1;
            if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                size = accessor.type;
            }

            int vaa = -1;
            if (attrib.first.compare("POSITION") == 0) vaa = 0;
            if (attrib.first.compare("NORMAL") == 0) vaa = 1;
            if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
            if (vaa > -1) {
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                    accessor.normalized ? GL_TRUE : GL_FALSE,
                    byteStride, BUFFER_OFFSET(accessor.byteOffset));
            }
            else
                std::cout << "vaa missing: " << attrib.first << std::endl;
        }

        
    }
}

void GLTF_Model :: bindModelNodes(std::map<int, GLuint>& vaos, std::map<int, GLuint>& vbos, tinygltf::Model& model,
    tinygltf::Node& node) {
    // allocate vaos to each mesh
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        std::cout << std::endl;
        bindMesh(vbos, model, model.meshes[node.mesh]);
        std::cout << "mesh" << node.mesh << ": " << vao << std::endl;
        vaos[node.mesh] = vao;
    }

    for (size_t i = 0; i < node.children.size(); i++) {
        assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
        bindModelNodes(vaos, vbos, model, model.nodes[node.children[i]]);
    }
}

std::pair<std::map<int, GLuint>, std::map<int, GLuint>> GLTF_Model :: bindModel(tinygltf::Model& model) {
    std::map<int, GLuint> vbos;
    std::map<int, GLuint> vaos;

    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
        bindModelNodes(vaos, vbos, model, model.nodes[scene.nodes[i]]);
    }
    glBindVertexArray(0);

    // get lights imformation
    for (auto& node : model.nodes) {
        if (node.name.find("LightComponent0") != std::string::npos) {
            for (auto& childIndex : node.children) {
                auto& childNode = model.nodes[childIndex];
                if (childNode.light >= 0) {
                    auto& light = model.lights[childNode.light];
                    if (light.type == "point") {
                        PointLight pointLight;
                        //calculate the location
                        glm::vec3 rotationEuler = glm::vec3(node.rotation[0], node.rotation[1], node.rotation[2]);
                        glm::quat rotationQuat = glm::quat(rotationEuler);
                        glm::mat4 rotationMatrix = glm::mat4_cast(rotationQuat);
                        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), 
                            glm::vec3(node.translation[0], node.translation[1], node.translation[2])) * rotationMatrix;
                        pointLight.position = glm::vec3(modelMatrix[3]);

                        pointLight.color = glm::vec3(light.color[0], light.color[1], light.color[2]);
                        pointLight.intensity = light.intensity;
                        pointLights.push_back(pointLight);
                    }
                    else if (light.type == "directional") {
                        glm::quat q(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
                        //glm::quat rotationY = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                        glm::vec3 direction = q * glm::vec3(0.0f, 0.0f, -1.0f);

                        DirectionalLight directionalLight;
                        directionalLight.direction = direction;
                        directionalLight.color = glm::vec3(light.color[0], light.color[1], light.color[2]);
                        directionalLight.intensity = light.intensity;
                        directionalLights.push_back(directionalLight);
                    }
                }
            }
        }
    }

    //bind textures
    //resize my textureID size
    textureIDs.resize(model.textures.size());

    //load all textures in a model
    for (size_t i = 0; i < model.textures.size(); i++) {
        tinygltf::Texture& tex = model.textures[i];

        if (tex.source > -1) {
            GLuint texid;
            glGenTextures(1, &texid);

            tinygltf::Image& image = model.images[tex.source];

            glBindTexture(GL_TEXTURE_2D, texid);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            GLenum format = GL_RGBA;

            if (image.component == 1) {
                format = GL_RED;
            }
            else if (image.component == 2) {
                format = GL_RG;
            }
            else if (image.component == 3) {
                format = GL_RGB;
            }
            GLenum type = GL_UNSIGNED_BYTE;
            if (image.bits == 8) {

            }
            else if (image.bits == 16) {
                type = GL_UNSIGNED_SHORT;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                format, type, &image.image.at(0));
            // Store the texture ID
            textureIDs[i] = texid;
        }
    }

    return { vaos, vbos };
}

void GLTF_Model :: drawMesh(const std::map<int, GLuint>& vbos,
    tinygltf::Model& model, tinygltf::Mesh& mesh, Shader& shader) {
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive& primitive = mesh.primitives[i];

        //initialize factors
        baseColorFactor = glm::vec4(-1.0f, -1.0f, -1.0f, -1.0f);
        metallicFactor = -1.0f;
        roughnessFactor = -1.0f;
        
        // bind indices VBO
        tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

        // bind attributes VBOs and configure vertex attribute pointers
        for (auto& attrib : primitive.attributes) {
            int accessorIdx = attrib.second;
            tinygltf::Accessor& accessor = model.accessors[accessorIdx];
            glBindBuffer(GL_ARRAY_BUFFER, vbos.at(accessor.bufferView));
        }
        //bind all textures
        int materialIndex = primitive.material;
        if (materialIndex >= 0) {
            
            tinygltf::Material& material = model.materials[materialIndex];
            // find textures
            // diffuse/basecolor
            if (material.values.find("baseColorTexture") != material.values.end()) {
                int textureIndex = material.values["baseColorTexture"].TextureIndex();
                GLuint textureId = textureIDs[textureIndex];
                shader.setVec4("baseColorFactor", baseColorFactor);
                //use Texture
                //active texture
                int textureUnitIndex = textureUnitIndices["baseColorTexture"];
                glActiveTexture(GL_TEXTURE0 + textureUnitIndex);

                // now set the sampler to the correct texture unit
                glUniform1i(glGetUniformLocation(shader.ID, "baseColorTexture"), textureUnitIndex);
                // and finally bind the texture
                glBindTexture(GL_TEXTURE_2D, textureId);
            }
            else if (material.values.find("baseColorFactor") != material.values.end()) {
                //use basecolor factor
                tinygltf::Parameter param = material.values["baseColorFactor"];
                baseColorFactor = glm::vec4(param.number_array[0], param.number_array[1], param.number_array[2], param.number_array[3]);
                //set baseColorFactor uniform
                shader.setVec4("baseColorFactor", baseColorFactor);
            }
            // metallicRoughnessTexture
            if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
                int textureIndex = material.values["metallicRoughnessTexture"].TextureIndex();
                GLuint textureId = textureIDs[textureIndex];

                //active texture
                int textureUnitIndex = textureUnitIndices["metallicRoughnessTexture"];
                glActiveTexture(GL_TEXTURE0 + textureUnitIndex);

                // now set the sampler to the correct texture unit
                glUniform1i(glGetUniformLocation(shader.ID, "metallicRoughnessTexture"), textureUnitIndex);
                // and finally bind the texture
                glBindTexture(GL_TEXTURE_2D, textureId);
            }
            else {
                if (material.values.find("metallicFactor") != material.values.end()) {
                    tinygltf::Parameter metallicParam = material.values["metallicFactor"];
                    metallicFactor = metallicParam.number_value;
                    //set metallic texture
                    shader.setFloat("metallicFactor", metallicFactor);
                }
                if (material.values.find("roughnessFactor") != material.values.end()) {
                    tinygltf::Parameter roughnessParam = material.values["roughnessFactor"];
                    roughnessFactor = roughnessParam.number_value;
                    //set roughness texture
                    shader.setFloat("roughnessFactor", roughnessFactor);
                }
            }
            // normalTexture
            if (material.additionalValues.find("normalTexture") != material.additionalValues.end()) {
                int textureIndex = material.additionalValues["normalTexture"].TextureIndex();
                GLuint textureId = textureIDs[textureIndex];
                
                //active texture
                int textureUnitIndex = textureUnitIndices["normalTexture"];
                glActiveTexture(GL_TEXTURE0 + textureUnitIndex);

                // now set the sampler to the correct texture unit
                glUniform1i(glGetUniformLocation(shader.ID, "normalTexture"), textureUnitIndex);
                // and finally bind the texture
                glBindTexture(GL_TEXTURE_2D, textureId);
            }
            // occlusionTexture
            if (material.additionalValues.find("occlusionTexture") != material.additionalValues.end()) {
                int textureIndex = material.additionalValues["occlusionTexture"].TextureIndex();
                GLuint textureId = textureIDs[textureIndex];
                
                //active texture
                int textureUnitIndex = textureUnitIndices["occlusionTexture"];
                glActiveTexture(GL_TEXTURE0 + textureUnitIndex);

                // now set the sampler to the correct texture unit
                glUniform1i(glGetUniformLocation(shader.ID, "occlusionTexture"), textureUnitIndex);
                // and finally bind the texture
                glBindTexture(GL_TEXTURE_2D, textureId);
            }
        }

        // draw elements
        glDrawElements(primitive.mode, indexAccessor.count, indexAccessor.componentType,
            BUFFER_OFFSET(indexAccessor.byteOffset));
    }
}

// recursively draw node and children nodes of model
void GLTF_Model :: drawModelNodes(const std::pair<std::map<int, GLuint>, std::map<int, GLuint>> VaosAndEbos,
    tinygltf::Model& model, tinygltf::Node& node, Shader& shader) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        //set model matrix for every mesh
        float rotationSpeed = 0.5f;
        float angle = glfwGetTime() * rotationSpeed; // Use the elapsed time to calculate the angle
        glm::mat4 Model = getModelMatrix(node, angle);
        shader.use();
        shader.setMat4("model",Model);

        //bind vao for every model
        glBindVertexArray(VaosAndEbos.first.at(node.mesh));
        drawMesh(VaosAndEbos.second, model, model.meshes[node.mesh], shader);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(VaosAndEbos, model, model.nodes[node.children[i]], shader);
    }
}
void GLTF_Model :: drawModel(const std::pair<std::map<int, GLuint>, std::map<int, GLuint>> VaosAndEbos,
    tinygltf::Model& model, Shader& shader) {
    //set lights' number and attribut
    shader.use();
    shader.setInt("numPointLights", pointLights.size());
    shader.setInt("numDirLights", directionalLights.size());
    std::string number;
    for (int i = 0; i < pointLights.size(); i++) {
        number = std::to_string(i);
        shader.setVec3(("pointLight["+ number + "].position").c_str(), pointLights[i].position);
        shader.setVec3(("pointLight[" + number + "].color").c_str(), pointLights[i].color);
        shader.setFloat(("pointLight[" + number + "].intensity").c_str(), pointLights[i].intensity);
    }
    for (int i = 0; i < directionalLights.size(); i++) {
        number = std::to_string(i);
        //shader.setVec3(("directionalLight[" + number + "].direction").c_str(), glm::vec3(0.0f,-0.5f,0.5f));
        shader.setVec3(("directionalLight[" + number + "].direction").c_str(), directionalLights[i].direction);
        shader.setVec3(("directionalLight[" + number + "].color").c_str(), directionalLights[i].color);
        shader.setFloat(("directionalLight[" + number + "].intensity").c_str(), directionalLights[i].intensity);
    }
    
    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        drawModelNodes(VaosAndEbos, model, model.nodes[scene.nodes[i]], shader);
    }
    glBindVertexArray(0);
}

void GLTF_Model :: dbgModel(tinygltf::Model& model) {
    for (auto& mesh : model.meshes) {
        std::cout << "mesh : " << mesh.name << std::endl;
        for (auto& primitive : mesh.primitives) {
            const tinygltf::Accessor& indexAccessor =
                model.accessors[primitive.indices];

            std::cout << "indexaccessor: count " << indexAccessor.count << ", type "
                << indexAccessor.componentType << std::endl;

            tinygltf::Material& mat = model.materials[primitive.material];
            for (auto& mats : mat.values) {
                std::cout << "mat : " << mats.first.c_str() << std::endl;
            }

            for (auto& image : model.images) {
                std::cout << "image name : " << image.uri << std::endl;
                std::cout << "  size : " << image.image.size() << std::endl;
                std::cout << "  w/h : " << image.width << "/" << image.height
                    << std::endl;
            }

            std::cout << "indices : " << primitive.indices << std::endl;
            std::cout << "mode     : "
                << "(" << primitive.mode << ")" << std::endl;

            for (auto& attrib : primitive.attributes) {
                std::cout << "attribute : " << attrib.first.c_str() << std::endl;
            }
        }
    }
}

void GLTF_Model :: draw(Shader& shader) {
    drawModel(VaosAndEbos, model, shader);
}

void GLTF_Model::bindVbo(std::map<int, GLuint>& vbos, tinygltf::Model& model, size_t indice) {
    //bind indice vbo
    tinygltf::BufferView& bufferView = model.bufferViews[indice];
    if (bufferView.target != GL_ARRAY_BUFFER && bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
        std::cout << "WARN: bufferView.target is neither GL_ARRAY_BUFFER nor GL_ELEMENT_ARRAY_BUFFER" << std::endl;
        exit(1);  // Skip unsupported bufferView.
    }
    std::cout << "vbo's key: " << indice << std::endl;
    std::cout << " bufferView.byteLength: " << bufferView.byteLength
        << ", bufferview.byteOffset = " << bufferView.byteOffset
        << std::endl;

    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
    GLuint vbo;
    glGenBuffers(1, &vbo);
    vbos[indice] = vbo;
    glBindBuffer(bufferView.target, vbo);

    glBufferData(bufferView.target, bufferView.byteLength,
        &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
}

glm::mat4 GLTF_Model::getModelMatrix(tinygltf::Node& node, float globalAngle)
{
    glm::mat4 modelMatrix(1.0f);

    if (node.matrix.size() == 16) // if Matrix is provided
    {
        // Note: tinygltf uses row-major format so you need to convert it to column-major format which glm uses.
        modelMatrix = glm::make_mat4(node.matrix.data());
        modelMatrix = glm::transpose(modelMatrix);
    }
    else // Need to construct matrix from position, rotation, scale
    {
        // Create translation matrix
        if (node.translation.size() == 3)
        {
            modelMatrix = glm::translate(modelMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }

        // Create rotation matrix
        if (node.rotation.size() == 4)
        {
            glm::quat q;
            q.x = node.rotation[0];
            q.y = node.rotation[1];
            q.z = node.rotation[2];
            q.w = node.rotation[3];

            modelMatrix *= glm::mat4_cast(q);
        }

        // Create scale matrix
        if (node.scale.size() == 3)
        {
            modelMatrix = glm::scale(modelMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }

    // if rotate the scene
    //glm::mat4 globalRotation = glm::rotate(glm::mat4(1.0f), globalAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    //modelMatrix = globalRotation * modelMatrix;

    return modelMatrix;
}