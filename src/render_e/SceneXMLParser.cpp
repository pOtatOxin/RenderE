/*
 *  RenderE
 *
 *  Created by Morten Nobel-Jørgensen ( http://www.nobel-joergnesen.com/ ) 
 *  License: LGPL 3.0 ( http://www.gnu.org/licenses/lgpl-3.0.txt )
 */

#include "SceneXMLParser.h"

#include <sstream>
#include <stack>
#include <map>

// Mandatory for using any feature of Xerces.
#include <xercesc/util/PlatformUtils.hpp>

// DOM (if you want SAX, then that's a different include)
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>

#include "math/Mathf.h"
#include "shaders/ShaderFileDataSource.h"
#include "textures/Texture2D.h"
#include "textures/CubeTexture.h"
#include "Material.h"
#include "Camera.h"
#include "FBXLoader.h"
#include "MeshFactory.h"
#include "Light.h"
#include "RenderBase.h"
#include "Log.h"

// define xerces namespace
XERCES_CPP_NAMESPACE_USE
using namespace std;


namespace render_e {

// Helper functions

bool stringEqual(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

float stringToFloat(const char *s1) {
    return (float) atof(s1);
}

int stringToInt(const char *s1) {
    return atoi(s1);
}

glm::vec3 stringToVector3(const char *s1) {
    static char tmp[512];
    strncpy (tmp, s1, 512);
    
    glm::vec3 f;
    char * pch;
    int index = 0;
    pch = strtok(tmp, ",");
    while (pch != NULL && index < 3) {
        f[index] = (float) atof(pch);
        index++;
        pch = strtok(NULL, ",");
    }

    return f;
}

glm::vec4 stringToVector4(const char *s1) {
    static char tmp[512];
    strncpy (tmp, s1, 512);
    
    glm::vec4 f;
    char * pch;
    int index = 0;
    pch = strtok(tmp, ",");
    while (pch != NULL && index < 4) {
        f[index] = (float) atof(pch);
        index++;
        pch = strtok(NULL, ",");
    }

    return f;
}

glm::vec2 stringToVector2(const char *s1) {
    static char tmp[512];
    strncpy (tmp, s1, 512);
    
    glm::vec2 f;
    char * pch;
    int index = 0;
    pch = strtok(tmp, ",");
    while (pch != NULL && index < 2) {
        f[index] = (float) atof(pch);
        index++;
        pch = strtok(NULL, ",");
    }
 
    return f;
}

enum MyParserState {
    SCENE,
    SHADERS,
    TEXTURES,
    MATERIALS,
    SCENEOBJECTS,
    SCENEOBJECT,
    COMPONENT
};

// internal helper classes

class MySAXHandler : public HandlerBase {
public:

    MySAXHandler(RenderBase *renderBase) : renderBase(renderBase), sceneObject(NULL) {
    }
    
    void error(const char *tagName){
        stringstream ss;
        ss<<"Unknown tag "<<tagName<<" state "<<state.top();
        ERROR(ss.str());
    }

    void parseScene(const XMLCh * const name, AttributeList& attributes, const char* message) {
        if (stringEqual("shaders", message)) {
            state.push(SHADERS);
        } else if (stringEqual("materials", message)) {
            state.push(MATERIALS);
        } else if (stringEqual("textures", message)) {
            state.push(TEXTURES);
        } else if (stringEqual("scenegraph", message)) {
            state.push(SCENEOBJECTS);
        } else {
            error(message);
        }
        
    }

    void parseShaders(const XMLCh * const name, AttributeList& attributes, const char* message) {
        if (stringEqual("shader", message)) {
            string shaderName;
            string file;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    shaderName.append(attValue);
                } else if (stringEqual("file", attName)) {
                    file.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown shader attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            ShaderLoadStatus status;
            
            Shader *shader = renderBase->CreateShader(file, shaderName, renderBase->GetShaderDataSource(), status);
            if (status==SHADER_OK){
                stringstream ss;
                ss << "Loaded shader "<<shaderName;
                INFO(ss.str());
            } else {
                stringstream ss;
                ss << "Cannot load shader "<<file;
                ERROR(ss.str());
            }
        } else {
            error(message);
        }
    }

    void parseTextures(const XMLCh * const name, AttributeList& attributes, const char* message) {
        if (stringEqual("texture2d", message)) {
            string textureName;
            string file;
            int width;
            int height;
            string type;
            bool clamp = true;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    textureName.append(attValue);
                } else if (stringEqual("file", attName)) {
                    file.append(attValue);
                } else if (stringEqual("type", attName)) {
                    type.append(attValue);
                } else if (stringEqual("width", attName)) {
                    width = stringToInt(attValue);
                } else if (stringEqual("height", attName)) {
                    height = stringToInt(attValue);
                } else if (stringEqual("clamp", attName)) {
                    clamp = stringEqual("clamp", attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown texture2d attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            if (file.length()>0){
                stringstream ss;
                ss << "Loading texture "<<(file.c_str());
                INFO(ss.str());
                Texture2D *texture = new Texture2D(file.c_str());
                texture->SetName(textureName);
                texture->SetClamp(clamp);
                TextureLoadStatus status = texture->Load();
                if (status == OK){
                    textures[textureName] = texture;
                } else {
                    ss.seekp(0);
                    ss<<"Error loading texture "<<textureName<<" filename "<<file;
                    ERROR(ss.str());
                }
            } else {
                Texture2D *texture = new Texture2D();
                texture->SetClamp(clamp);
                texture->SetName(textureName);
                TextureFormat textureFormat;
                if (type.compare("DEPTH")==0){
                    textureFormat = DEPTH;
                } else if (type.compare("RGB")==0){
                    textureFormat = RGB;
                } else {
                    textureFormat = RGBA;
                }
                texture->Create(width, height, textureFormat);
                textures[textureName] = texture;
            }
        } else if (stringEqual("cubetexture", message)) {
            string textureName;
            string left, right, top, bottom, back, front;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    textureName.append(attValue);
                } else if (stringEqual("left", attName)) {
                    left.append(attValue);
                } else if (stringEqual("right", attName)) {
                    right.append(attValue);
                } else if (stringEqual("top", attName)) {
                    top.append(attValue);
                } else if (stringEqual("bottom", attName)) {
                    bottom.append(attValue);
                } else if (stringEqual("back", attName)) {
                    back.append(attValue);
                } else if (stringEqual("front", attName)) {
                    front.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown cubetexture attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            CubeTexture *texture = new CubeTexture(
                    left.c_str(), right.c_str(),
                    top.c_str(), bottom.c_str(),
                    back.c_str(), front.c_str());
            texture->SetName(textureName);
            TextureLoadStatus status = texture->Load();
            if (status == OK){
                textures[textureName] = texture;
            } else {
                stringstream ss;
                ss<<"Error loading cube texture "<<textureName<<" filename "<<left;
                ERROR(ss.str());
            }
        } else {
            error(message);
        }
    }

    void parseMaterials(const XMLCh * const name, AttributeList& attributes, const char* message) {
        static Material *material = NULL;
        if (stringEqual("material", message)) {
            string matName;
            string shader;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    matName.append(attValue);
                } else if (stringEqual("shader", attName)) {
                    shader.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            
            Shader* shaderObj = renderBase->GetShader(shader);
            if (shaderObj == NULL) {
                stringstream ss;
                ss << "Cannot find shader " << shader;
                ERROR(ss.str());
            } else {
                material = new Material(shaderObj);
                material->SetName(matName);
                materials[matName] = material;
            }
        } else if (stringEqual("parameter", message)) {
            assert(material != NULL);
            string parameterName;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
				if (stringEqual("name", attName)) {
                    parameterName.append(attValue);
                } else if (stringEqual("vector2", attName)) {
                    material->SetVector2(parameterName, stringToVector2(attValue));
                } else if (stringEqual("vector3", attName)) {
                    material->SetVector3(parameterName, stringToVector3(attValue));
                } else if (stringEqual("cameraRef", attName)) {
                    material->SetShadowSetup(parameterName, attValue);
				} else if (stringEqual("vector4", attName)) {
                    material->SetVector4(parameterName, stringToVector4(attValue));
                } else if (stringEqual("texture", attName)) {
                    map<string, TextureBase*>::iterator iter = textures.find(attValue);
                    if (iter == textures.end()) {
                        stringstream ss;
                        ss << "Cannot find texture " << attValue;
                        ERROR(ss.str());
                    } else {
                        material->SetTexture(parameterName, iter->second);
                    }
                } else if (stringEqual("float", attName)) {
                    float f = stringToFloat(attValue);
                    material->SetFloat(parameterName, f);
                } else if (stringEqual("int", attName)) {
                    int i = stringToInt(attValue);
                    material->SetInt(parameterName, i);
                } else {
                    stringstream ss;
                    ss << "Unknown parameter attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
        } else {
            error(message);
        }
    }

    void parseSceneObjects(const XMLCh * const name, AttributeList& attributes, const char* message) {
        if (stringEqual("object", message)) {
            string objectName;
            glm::vec3 position;
            glm::vec3 rotation;
            glm::vec3 scale(1,1,1);
            string parent;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    objectName.append(attValue);
                } else if (stringEqual("position", attName)) {
                    position = stringToVector3(attValue);
                } else if (stringEqual("rotation", attName)||stringEqual("rotate", attName)) {
                    rotation = stringToVector3(attValue)*Mathf::DEGREE_TO_RADIAN;
                } else if (stringEqual("scale", attName)) {
                    scale = stringToVector3(attValue);
                } else if (stringEqual("parent", attName)) {
                    parent.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown object attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            sceneObject = new SceneObject();
            sceneObject->GetTransform()->SetPosition(position);
            sceneObject->GetTransform()->SetRotation(rotation);
            sceneObject->GetTransform()->SetScale(scale);
            sceneObject->SetName(objectName);
            if (objectName.length() > 0 && parent.length() > 0){
                parentMap[objectName] = parent;
            }
        } else {
            error(message);
        }
    }
    
    void parseComponents(const XMLCh * const name, AttributeList& attributes, const char* message) {
        if (stringEqual("camera", message)) {
            Camera *cam = new Camera();
            bool projection = true;
            float fieldOfView = 40.0f;
            float aspect = 1.0f;
            float nearPlane = 0.1f;
            float farPlane = 1000.0f;
            float left = -1;
            float right = 1;
            float bottom = -1;
            float top = 1;
            Texture2D *renderToTexture = NULL;
            CameraBuffer cameraBuffer; // used in renderToTexture
            /*renderToTexture="texture" renderBuffer="COLOR_BUFFER"*/
            glm::vec4 clearColor(0,0,0,1);
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));

                if (stringEqual("type", attName)){
                    if (stringEqual("orthographic",attValue)){
                        projection = false;
                    }
                } else if (stringEqual("renderToTexture",attName)){
                    map<string, TextureBase*>::iterator iter = textures.find(attValue);
                    if (iter == textures.end()) {
                        stringstream ss;
                        ss << "Cannot find texture " << attValue;
                        ERROR(ss.str());
                    } else {
                        renderToTexture = static_cast<Texture2D *>( iter->second);
                    }
                    
                } else if (stringEqual("renderBuffer",attName)){
                    if (stringEqual("COLOR_BUFFER", attValue)){
                        cameraBuffer = COLOR_BUFFER;
                    } else if (stringEqual("DEPTH_BUFFER", attValue)){
                        cameraBuffer = DEPTH_BUFFER;
                    } else if (stringEqual("STENCIL_BUFFER", attValue)){
                        cameraBuffer = STENCIL_BUFFER;
                    } else {
                        stringstream ss;
                        ss <<"Unknown type for renderBuffer - supported types are: COLOR_BUFFER, DEPTH_BUFFER, STENCIL_BUFFER. Actual value was "<<attValue;
                        ERROR(ss.str());
                    }
                } else if (stringEqual("fieldOfView",attName)){
                    fieldOfView = stringToFloat(attValue);
                } else if (stringEqual("aspect",attName)){
                    aspect = stringToFloat(attValue);
                } else if (stringEqual("nearPlane",attName)){
                    nearPlane = stringToFloat(attValue);
                } else if (stringEqual("farPlane",attName)){
                    farPlane = stringToFloat(attValue);
                } else if (stringEqual("left",attName)){
                    left = stringToFloat(attValue);
                } else if (stringEqual("right",attName)){
                    right = stringToFloat(attValue);
                } else if (stringEqual("bottom",attName)){
                    bottom = stringToFloat(attValue);
                } else if (stringEqual("top",attName)){
                    top = stringToFloat(attValue);
                } else if (stringEqual("nearPlane",attName)){
                    nearPlane = stringToFloat(attValue);
                } else if (stringEqual("farPlane",attName)){
                    farPlane = stringToFloat(attValue);
                } else if (stringEqual("clearColor",attName)){
                    clearColor = stringToVector4(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown camera attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            if (projection){
                cam->SetProjection(fieldOfView, aspect, nearPlane,farPlane);
            } else {
                cam->SetOrthographic(left, right, bottom, top, nearPlane,farPlane);
            }
            if (renderToTexture != NULL){
                cam->SetRenderToTexture(true, cameraBuffer, static_cast<Texture2D*>(renderToTexture));
            }
            cam->SetClearColor(clearColor);
            sceneObject->AddCompnent(cam);
        } else if (stringEqual("material", message)) {
            string ref;
            for (unsigned int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("ref", attName)) {
                    ref.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown material attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            if (ref.length()>0){
                map<string, Material*>::iterator iter = materials.find(ref);
                if (iter == materials.end()) {
                    stringstream ss;
                    ss << "Cannot find material " << ref;
                    ERROR(ss.str());
                } else {
					sceneObject->AddCompnent(iter->second->Instance());
                }
            } else {
                WARN("Warn material ref not set");
            }
        } else if (stringEqual("mesh", message)) {
            string meshName;
            string primitive;
            string import;
            for (int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("primitive", attName)) {
                    primitive.append(attValue);
                } else if (stringEqual("import", attName)) {
                    import.append(attValue);
                } else {
                    stringstream ss;
                    ss << "Unknown attribute name "<<attName;
                    ERROR(ss.str());
                }
                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            Mesh *mesh = NULL;
            if (primitive.length() > 0){
                if (stringEqual("cube", primitive.c_str())){
                    mesh = MeshFactory::CreateCube();
                } else if (stringEqual("sphere", primitive.c_str())){
                    mesh = MeshFactory::CreateICOSphere();
                } else if (stringEqual("tetrahedron", primitive.c_str())){
                    mesh = MeshFactory::CreateTetrahedron();
                } else if (stringEqual("plane", primitive.c_str())){
                    mesh = MeshFactory::CreatePlane();
                } else {
                    stringstream ss;
                    ss << "Unknown mesh.primitive name "<<primitive.c_str();
                    ERROR(ss.str());
                }
            } else if (import.length() > 0){
				MeshComponent *meshC = fbxLoader.LoadMeshComponent(import.c_str());
				if (meshC != NULL){
					meshC->SetOwner(NULL);
					sceneObject->AddCompnent(meshC);
				} else {
                    stringstream ss;
					ss << "Cannot find mesh in "<<import;
                    ERROR(ss.str());
				}
            }
            if (mesh != NULL){
                assert(mesh->IsValid());
                MeshComponent *meshComponent = new MeshComponent();
                meshComponent->SetMesh(mesh);
                sceneObject->AddCompnent(meshComponent);
            }
        } else if (stringEqual("light", message)){
            string lightName;
            Light *light = new Light();
            
            string lightType;
            for (int i = 0; i < attributes.getLength(); i++) {
                char *attName = XMLString::transcode(attributes.getName(i));
                char *attValue = XMLString::transcode(attributes.getValue(i));
                if (stringEqual("name", attName)) {
                    lightName.append(attValue);
                } else if (stringEqual("type", attName)) {
                    lightType.append(attValue);
                } else if (stringEqual("ambient", attName)) {
                    light->SetAmbient(stringToVector4(attValue));
                } else if (stringEqual("diffuse", attName)) {
                    light->SetDiffuse(stringToVector4(attValue));
                } else if (stringEqual("specular", attName)) {
                    light->SetSpecular(stringToVector4(attValue));
                } else if (stringEqual("type", attName)) {
                    if (stringEqual("point", attValue)){
                        light->SetLightType(PointLight);
					} else if (stringEqual("spot", attValue)){
                        light->SetLightType(SpotLight);
                    } else if (stringEqual("directional", attValue)){
                        light->SetLightType(DirectionalLight);
                    } else {
                        stringstream ss;
                        ss << "Unknown lighttype "<<attValue;
                        ERROR(ss.str());
                    }
                } else if (stringEqual("constantAttenuation", attName)) {
                    light->SetConstantAttenuation(stringToFloat(attValue));
                } else if (stringEqual("linearAttenuation", attName)) {
                    light->SetLinearAttenuation(stringToFloat(attValue));
                } else if (stringEqual("quadraticAttenuation", attName)) {
                    light->SetQuadraticAttenuation(stringToFloat(attValue));
                } else if (stringEqual("spotDirection", attName)) {
                    glm::vec3 v = stringToVector3(attValue);
                    light->SetSpotDirection(v);
                } else if (stringEqual("spotCutoff", attName)) {
					light->SetSpotCutoff(stringToInt(attValue));
                } else {
                    stringstream ss;
                    ss << "Unknown light attribute name "<<attName;
                    ERROR(ss.str());
                }

                XMLString::release(&attValue);
                XMLString::release(&attName);
            }
            
            sceneObject->AddCompnent(light);
        } else {
            error(message);
        }
    }
    
    void applyTransformHiarchy(){
        map<string, string>::iterator iter = parentMap.begin();
        for (;iter!=parentMap.end();iter++){
            string child = (iter)->first;
            string parent = (iter)->second;
            SceneObject *childO = renderBase->Find(child.c_str());
            SceneObject *parentO = renderBase->Find(parent.c_str());
            if (childO != NULL && parentO != NULL){
                parentO->AddChild(childO);
            } else {
                assert(childO != NULL); //Child should always be there
                stringstream ss;
                ss<<"Cannot find parent " <<
                    parent <<
                    " to child " <<
                    child;
                ERROR(ss.str());
            }
        }
    }

    void endElement(const XMLCh * const name) {
        assert(!state.empty());
        MyParserState prevState = state.top();
        state.pop();
        if (prevState == SCENEOBJECT){
            renderBase->AddSceneObject(sceneObject);
        } else if (prevState == SCENEOBJECTS){
            applyTransformHiarchy();
        }
    }

    void startElement(const XMLCh * const name, AttributeList& attributes) {
        
        char* message = XMLString::transcode(name);
        
        if (state.empty()) {
            state.push(SCENE);
        } else {
            switch (state.top()) {
                case SCENE:
                    parseScene(name, attributes, message);
                    break;
                case SHADERS:
                    parseShaders(name, attributes, message);
                    state.push(SHADERS); // push same value to stack
                    break;
                case TEXTURES:
                    parseTextures(name, attributes, message);
                    state.push(TEXTURES); // push same value to stack
                    break;
                case MATERIALS:
                    parseMaterials(name, attributes, message);
                    state.push(MATERIALS); // push same value to stack
                    break;
                case SCENEOBJECTS:
                    parseSceneObjects(name, attributes, message);
                    state.push(SCENEOBJECT); // push same value to stack
                    break;
                case SCENEOBJECT:
                    parseComponents(name, attributes, message);
                    state.push(COMPONENT); // push same value to stack
                    break;
            }
        }

        XMLString::release(&message);
    }

    void fatalError(const SAXParseException& exception) {
        char* message = XMLString::transcode(exception.getMessage());
        stringstream ss;
        ss << "Fatal Error: " << message
                << " at line: " << exception.getLineNumber();
        FATAL(ss.str());
        XMLString::release(&message);
    }

    SceneObject *sceneObject;
    RenderBase *renderBase;
    stack<MyParserState> state;

    FBXLoader fbxLoader;
    map<string, TextureBase*> textures;
    map<string, Material*> materials;
    map<string, string> parentMap;
};

SceneXMLParser::SceneXMLParser() {
    try {
        XMLPlatformUtils::Initialize();
    } catch (const XMLException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
        stringstream ss;
        ss << "Error during initialization! :\n"
                << message;
        ERROR(ss.str());
        XMLString::release(&message);
        // Do your failure processing here
        return;
    }
}

SceneXMLParser::~SceneXMLParser() {
    XMLPlatformUtils::Terminate();
}

void SceneXMLParser::LoadScene(const char* filename, RenderBase *renderBase) {
    SAXParser* parser = new SAXParser();
    parser->setDoNamespaces(true); // optional

    MySAXHandler* docHandler = new MySAXHandler(renderBase);
    ErrorHandler* errHandler = (ErrorHandler*) docHandler;
    parser->setDocumentHandler(docHandler);
    parser->setErrorHandler(errHandler);
    stringstream ss;
    ss<<"Loading scene "<<filename;
    INFO(ss.str());
    try {
        parser->parse(filename);
    } catch (const XMLException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
        ss.seekp(0);
        ss << "Exception message is: "<<endl
                << message;
        ERROR(ss.str());
        XMLString::release(&message);
        return;
    } catch (const SAXParseException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
        ss.seekp(0);
        ss << "Exception message is: "<<endl
                << message;
        ERROR(ss.str());
        XMLString::release(&message);
        return;
    } catch (...) {
        ERROR("Unexpected Exception");
        return;
    }
    delete parser;
    delete docHandler;
}
}

