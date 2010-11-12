/* 
 * File:   TextureBase.h
 * Author: morten
 *
 * Created on November 12, 2010, 11:50 AM
 */

#ifndef TEXTUREBASE_H
#define	TEXTUREBASE_H


namespace render_e {


enum TextureLoadStatus{
    ERROR,
    ERROR_READING_FILE,
    INVALID_FORMAT,
    OK
};

class TextureBase {
public:
    TextureBase(unsigned short textureType);
    virtual ~TextureBase();
    
    virtual TextureLoadStatus Load() = 0;
    void Bind();
    void Unbind();
    void Unload();
protected:
    unsigned short textureType;
    unsigned int textureId;
    bool mipmapping;
private:
    TextureBase(const TextureBase& orig); // disallow copy constructor
    TextureBase& operator = (const TextureBase&); // disallow copy constructor
};

}

#endif	/* TEXTUREBASE_H */

