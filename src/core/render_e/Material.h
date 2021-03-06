/*
 *  RenderE
 *
 *  Created by Morten Nobel-Jørgensen ( http://www.nobel-joergnesen.com/ ) 
 *  License: LGPL 3.0 ( http://www.gnu.org/licenses/lgpl-3.0.txt )
 */

#ifndef MATERIAL_H
#define	MATERIAL_H

#include <vector>
#include <string>
#include "shaders/Shader.h"
#include "textures/TextureBase.h"
#include "Component.h"
#include <glm/glm.hpp>

namespace render_e {

class Camera; // forward declaration

enum ShaderParamType{
    SPT_FLOAT,
    SPT_VECTOR2,
    SPT_VECTOR3,
    SPT_VECTOR4,
    SPT_INT,
    SPT_TEXTURE,
	SPT_SHADOW_SETUP,
	SPT_SHADOW_SETUP_NAME, // will allow setup material without camera is defined yet
};

struct ShaderParameters{
    int id;
    ShaderParamType paramType;
    union ShaderValue {
        float f[4];
        int integer[2]; // value, bind texturetype
		Camera *camera;
		char *cameraName;
    } shaderValue;
};

class Material : public Component{
public:
    Material(Shader *shader);
    virtual ~Material();
    void Bind();
    
    bool SetVector2(std::string name, glm::vec2 vec);
    bool SetVector3(std::string name, glm::vec3 vec);
    bool SetVector4(std::string name, glm::vec4 vec);
    bool SetFloat(std::string name, float f);
    bool SetTexture(std::string name, TextureBase *texture);
    bool SetInt(std::string name, int i);
	bool SetShadowSetup(std::string name, const char *cameraName);
    
    void SetName(std::string name) { this->name = name;}
    std::string GetName() {return name; }
	Material *Instance(); // create a copy of material
private:
    Material(const Material& orig); // disallow copy constructor
    Material& operator = (const Material&); // disallow copy constructor
    
    void AddParameter(ShaderParameters &param);
    
    Shader *shader;
    std::vector<TextureBase*> textures;    
    std::string name;
    std::vector<ShaderParameters> parameters;
};
}
#endif	/* MATERIAL_H */

