#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
// Needed on MsWindows
#include <windows.h>
#endif // Win32 platform

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
// Download glut from: http://www.opengl.org/resources/libraries/glut/
#include <GLUT/glut.h>

#include "float2.h"
#include "float3.h"
#include "stb_image.h"
#include "meshpack/Mesh.h"
#include <stdio.h>
#include <vector>
#include <map>

int dimension = 200;
static int score = 1;
class LightSource
{
public:
    virtual float3 getRadianceAt  ( float3 x )=0;
    virtual float3 getLightDirAt  ( float3 x )=0;
    virtual float  getDistanceFrom( float3 x )=0;
    virtual void   apply( GLenum openglLightName )=0;
};

class DirectionalLight : public LightSource
{
    float3 dir;
    float3 radiance;
public:
    DirectionalLight(float3 dir, float3 radiance)
    :dir(dir), radiance(radiance){}
    float3 getRadianceAt  ( float3 x ){return radiance;}
    float3 getLightDirAt  ( float3 x ){return dir;}
    float  getDistanceFrom( float3 x ){return 900000000;}
    void   apply( GLenum openglLightName )
    {
        float aglPos[] = {dir.x, dir.y, dir.z, 0.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
        float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
        float aglIntensity[] = {radiance.x, radiance.y, radiance.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.0f);
    }
};

class PointLight : public LightSource
{
    float3 pos;
    float3 power;
public:
    PointLight(float3 pos, float3 power)
    :pos(pos), power(power){}
    float3 getRadianceAt  ( float3 x ){return power*(1/(x-pos).norm2()*4*3.14);}
    float3 getLightDirAt  ( float3 x ){return (pos-x).normalize();}
    float  getDistanceFrom( float3 x ){return (pos-x).norm();}
    void   apply( GLenum openglLightName )
    {
        float aglPos[] = {pos.x, pos.y, pos.z, 1.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
        float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
        float aglIntensity[] = {power.x, power.y, power.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.25f / 3.14f);
    }
};

class Material
{
public:
    float3 kd;			// diffuse reflection coefficient
    float3 ks;			// specular reflection coefficient
    float shininess;	// specular exponent
    Material()
    {
        kd = float3(0.5, 0.5, 0.5) + float3::random() * 0.5;
        ks = float3(1, 1, 1);
        shininess = 15;
    }
    virtual void apply()
    {
        glDisable(GL_TEXTURE_2D);
        float aglDiffuse[] = {kd.x, kd.y, kd.z, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, aglDiffuse);
        float aglSpecular[] = {kd.x, kd.y, kd.z, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, aglSpecular);
        if(shininess <= 128)
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
        else
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0f);
    }
};

class TexturedMaterial : public Material{
    unsigned int textureName;
public:
    TexturedMaterial(const char* filename,
                     GLint filtering = GL_LINEAR_MIPMAP_LINEAR
                     ){
        unsigned char* data;
        int width;
        int height;
        int nComponents = 4;
        data = stbi_load(filename, &width, &height, &nComponents,
                         0);
        if(data == NULL) return;
        
        glGenTextures(1, &textureName);  // id generation
        
        glBindTexture(GL_TEXTURE_2D, textureName);      // binding
        if(nComponents == 4)
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height,
                              GL_RGBA, GL_UNSIGNED_BYTE, data);
        else if(nComponents == 3)
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height,
                              GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexEnvi(GL_TEXTURE_ENV,
                  GL_TEXTURE_ENV_MODE, GL_REPLACE);
        delete data;
    }
    
    void apply(){
        this->Material::apply();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureName);
    }
    


};

class Camera
{
    float3 eye;
    
    float3 ahead;
    float3 lookAt;
    float3 right;
    float3 up;
    
    float fov;
    float aspect;
    friend class Billboard;
    friend class ScoreCount;
    float2 lastMousePos;
    float2 mouseDelta;
    
public:
    float3 getEye()
    {
        return eye;
    }
    Camera()
    {
        eye = float3(0, 2, -5);
        lookAt = float3(0, 2, 0);
        right = float3(1, 0, 0);
        up = float3(0, 1, 0);
        
        fov = 1.1;
        aspect  = 1;
    }
    
    void apply()
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov /3.14*180, aspect, 0.1, 500);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye.x, eye.y, eye.z, lookAt.x, lookAt.y, lookAt.z, 0.0, 1.0, 0.0);
    }
    
    void setAspectRatio(float ar) { aspect= ar; }
    
    void move(float3 position, float orientation, float dt, std::vector<bool>& keysPressed)
    {
        
        
        ahead.x = cos(2*M_PI*(-orientation + 90)/360);
        ahead.y = 0;
        ahead.z = sin(2*M_PI*(-orientation + 90)/360);
        right = ahead.cross(float3(0, 1, 0)).normalize();
        up = right.cross(ahead);
        eye = position - ahead*20 + up*5;
        lookAt = eye + ahead;
    }
    
    void startDrag(int x, int y)
    {
        lastMousePos = float2(x, y);
    }
    void drag(int x, int y)
    {
        float2 mousePos(x, y);
        mouseDelta = mousePos - lastMousePos;
        lastMousePos = mousePos;
    }
    void endDrag()
    {
        mouseDelta = float2(0, 0);
    }
    
};

void * font= GLUT_BITMAP_TIMES_ROMAN_24;

void renderBitmapString(float x, float y, float z, void *font, const char *string){
    const char *c;
    glRasterPos3f(x, y, z);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    
    for (c=string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

class ScoreCount
{
    float3 position;
    float orientationAngle;
    
public:
    
    virtual void draw(Camera& c){
        
        char string[10];
        sprintf(string, "%d", score);
        position = c.lookAt + c.right*.5 + c.up*.5;
        renderBitmapString(position.x,position.y, position.z, font, string);
    }
};


class Billboard{
    float3 position;
    Material* material;
public:
    Billboard(Material* m){
        material = m;
    }
    
    void setPosition(float3 p){
        position = p;
    }
    
    virtual void draw(Camera& c){
        
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        
        material->apply();
        
        glPushMatrix();
        glTranslatef(position.x,position.y,position.z);
        glScalef(5,5,5);
        float camRotation[] = {
            c.right.x, c.right.y, c.right.z, 0,
            c.up.x, c.up.y, c.up.z, 0,
            c.ahead.x, c.ahead.y, c.ahead.z, 0,
            0, 0, 0, 1 };
        glMultMatrixf(camRotation);
        glColor4d(1,1,1,1);
        
        
        glBegin(GL_QUADS);
        glTexCoord3d(0,0,0);
        glVertex3f(-1, -1, 0);
        
        glTexCoord3d(0,1,0);
        glVertex3f(-1, 1, 0);
        
        glTexCoord3d(1,1,0);
        glVertex3f(1, 1, 0);
        
        glTexCoord3d(1,0,0);
        glVertex3f(1, -1, 0);
        glEnd();
        glPopMatrix();
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_LIGHTING);

    }

};

ScoreCount sc;

class Object
{
protected:
    Material* material;
    float3 scaleFactor;
    float3 position;
    float3 orientationAxis;
    float orientationAngle;
    bool isDead = false;
    bool isHazard = false;
    bool isEnemy = false;
    bool isTeapot = false;
    bool isAvatar = false;
    
public:
    Object(Material* material):material(material),orientationAngle(0.0f),scaleFactor(1.0,1.0,1.0),orientationAxis(0.0,1.0,0.0){}
    virtual ~Object(){}
    Object* translate(float3 offset){
        position += offset; return this;
    }
    Object* scale(float3 factor){
        scaleFactor *= factor; return this;
    }
    Object* rotate(float angle){
        orientationAngle += angle; return this;
    }
    
    float3 getPosition(){
        return position;
    }
    
    float3 setPosition(float3 p){
        return position = p;
    }

    
    float getOrientation(){
        return orientationAngle;
    }
    
    bool getIsHazard(){
        return isHazard;
    }
    bool getIsEnemy(){
        return isEnemy;
    }
    bool getIsTeapot(){
        return isTeapot;
    }
    bool getIsDead(){
        return isDead;
    }
    
    bool getIsAvatar(){
        return isAvatar;
    }
    
    virtual void drawShadow(float3 lightDir){
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glColor3f(0.1, 0.1, 0.1);
        
        if(isDead){
            material->apply();
        }
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        
        glTranslatef(1,0,1);
        glScalef(1,.01,1);
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
        glPopMatrix();
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
        
    }
    
    virtual void draw()
    {
        if(!isDead){
        material->apply();
        // apply scaling, translation and orientation
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
        glPopMatrix();
        }
    }
    virtual void drawModel()=0;
    virtual void move(double t, double dt){}
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects,std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    {return false;}
    virtual void dead(){}
    

};

class Bullet : public Object{
    float3 velocity;
public:
    Bullet(Material* m, float o, float3 p) : Object(m){
        position = p;
        float speed = 100;
        orientationAngle = o;
        
        float3 ahead;
        ahead.x = cos(M_PI*(-orientationAngle + 90)/180);
        ahead.y = 0;
        ahead.z = sin(M_PI*(-orientationAngle + 90)/180);
        
        velocity = ahead*speed;

    }
    
    virtual void move(double t, double dt){
        position += velocity*dt;
    }
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects,std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    {
        for(int i = 0; i < objects.size();i++)
        {
            if(this != objects.at(i) && !objects.at(i)->getIsAvatar() && (position - objects.at(i)->getPosition()).norm() < 5){
                if(objects.at(i)->getIsEnemy() && !objects.at(i)->getIsDead()){
                    Object* dead = objects.at(i);
                    dead->dead();
                    isDead = true;
                }
                if(!objects.at(i)->getIsDead() && (objects.at(i)->getIsEnemy() || objects.at(i)->getIsTeapot() || objects.at(i)->getIsHazard())){
                    isDead = true;
                }
            }
        }
        
        if(position.x > dimension){
            isDead = true;
        }
        if(position.x < -dimension){
            isDead = true;
        }
        if(position.z > dimension){
            isDead = true;
        }
        if(position.z < -dimension){
            isDead = true;
        }
        
        return false;
    }
    
    void draw()
    {
        glDisable(GL_LIGHTING);
        material->apply();
        // apply scaling, translation and orientation
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
        glPopMatrix();
        glEnable(GL_LIGHTING);
    }
    
    void drawModel(){
        glutSolidSphere(.2,50,50);
        
        
    }

};




class MeshInstance : public Object{
protected:
    Mesh* mesh;
public:
    MeshInstance(Mesh* me, Material* ma) : mesh(me), Object(ma) {}
    
    virtual void drawModel(){
        mesh->draw();
    }
    
    virtual void move(double t, double dt){}
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects,std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    { return false;}

};

class Tree : public MeshInstance{
public:
    Tree(Mesh* me, Material* ma) : MeshInstance(me, ma) {
        isHazard = true;
    }
};



class Seeker : public MeshInstance{
    float3 velocity;
    Object* desired;
public:
    Seeker(Mesh* me, Material* ma,Object* d) : MeshInstance(me,ma){
        isEnemy = true;
        desired = d;
    }
    
    virtual void move(double t, double dt){
        velocity = (desired->getPosition() - position).normalize()*score;
        float3 direction = (desired->getPosition() - position).normalize();

        orientationAngle = ((180*acos(direction.z))/M_PI);
        if(direction.x < 0)
            orientationAngle = 360 - orientationAngle;
        
        orientationAngle += 90;
        if(!isDead)
            position += velocity*dt;
    }
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects)
    {
        if(position.x > dimension){
            position.x = dimension;
            velocity.x *= -1;
        }
        if(position.x < -dimension){
            position.x = -dimension;
            velocity.x *= -1;
        }
        if(position.z > dimension){
            position.z = dimension;
            velocity.z *= -1;
        }
        if(position.z < -dimension){
            position.z = -dimension;
            velocity.z *= -1;
        }
        
        return false;
    }
    
    virtual void dead(){
        velocity = float3(0,0,0);
        isDead = true;
    }
};

class Teapot : public Object
{
public:
    Teapot(Material* material):Object(material){
        isTeapot = true;
    }
    
    void drawModel()
    {
        glutSolidTeapot(1.0f);
    }
    bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects, std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    {
        for(int i = 0; i < objects.size();i++)
        {
            if(this != objects.at(i) && !objects.at(i)->getIsEnemy() && (position - objects.at(i)->getPosition()).norm() < 5){
                position = position.random()*dimension;
                position.y = 1;
                if(objects.at(i)->getIsAvatar()){
                    for(int i = 0; i < score; i++){
                        Seeker* s = new Seeker(meshs.at(0), materials.at(0),objects.at(0));
                        s->scale(float3(.2,.2,.2));
                        float randomPos = float3().random().x*2*dimension - dimension;
                        float3 spawnPos;
                        if((score+i)%4 == 0)
                            spawnPos = float3(dimension,0,randomPos);
                        else if((score+i)%4 == 1)
                            spawnPos = float3(-dimension,0,randomPos);
                        else if((score+i)%4 == 2)
                            spawnPos = float3(randomPos,0,dimension);
                        else if((score+i)%4 == 3)
                            spawnPos = float3(randomPos,0,-dimension);
                        
                        s->setPosition(spawnPos);
                        spawn.push_back(s);
                    }
                    score++;
                }
            }
        }
        
        return false;
    }
    
};


class Avatar : public MeshInstance{
    float3 velocity;
    float acceleration;
    float speed;
    float angularVelocity;
    float angularAcceleration;
    bool isFiring = false;
public:
    Avatar(Mesh* me, Material* ma) : MeshInstance(me,ma){
        isAvatar = true;
        position = float3(0,1.5,0);
        speed = 0;
        angularVelocity = 0;
    }
    
    virtual void move(double t, double dt){
        
        speed += acceleration*dt;
        orientationAngle += angularVelocity*dt;
        float3 ahead;
        ahead.x = cos(M_PI*(-orientationAngle + 90)/180);
        ahead.y = 0;
        ahead.z = sin(M_PI*(-orientationAngle + 90)/180);
        float3 right = ahead.cross(float3(0, 1, 0)).normalize();
        
        velocity = ahead*speed;

        
        position += velocity*dt;
        if(position.y < 0){
            position.y = 0;
        }
        
        angularVelocity *= pow(0.4, dt);
        speed *= pow(0.7, dt);
        
        if(position.x > dimension){
            position.x = dimension;
            velocity.x *= -.1;
        }
        if(position.x < -dimension){
            position.x = -dimension;
            velocity.x *= -.1;
        }
        if(position.z > dimension){
            position.z = dimension;
            velocity.z *= -.1;
        }
        if(position.z < -dimension){
            position.z = -dimension;
            velocity.z *= -.1;
        }
        
    }
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects,std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    {
        int SPEED_MAX = 40;
        
        if(keysPressed.at('a'))
            angularVelocity += 2;
        if(keysPressed.at('d'))
            angularVelocity -= 2;
        if(keysPressed.at('w'))
            acceleration = 20;
        if(keysPressed.at('s'))
            acceleration = -20;
        if(keysPressed.at(' ') && !isFiring){
            Bullet* b = new Bullet(material,orientationAngle,position);
            spawn.push_back(b);
            isFiring = true;
        }
        if(!keysPressed.at(' ')){
            isFiring = false;
        }
        
        if(!keysPressed.at('w') & !keysPressed.at('s'))
            acceleration = 0;

 
        
        
        for(int i = 0; i < objects.size(); i++){
            if(objects.at(i)->getIsHazard() && (position - objects.at(i)->getPosition()).norm() < 5){
                speed = -speed;
            }
            
            if(!objects.at(i)->getIsDead() && objects.at(i)->getIsEnemy() && (position - objects.at(i)->getPosition()).norm() < 5){
                if((speed >= SPEED_MAX || speed <= -SPEED_MAX)){
                    speed = 0;
                    Object* dead = objects.at(i);
                    dead->dead();
                }
                else{
                    isDead = true;
                }
            }
        }
        
        //velocity = ahead*speed;
        
        return false;
    }
};

class Bouncer : public MeshInstance{
    float3 velocity;
    float angularVelocity;
    float angularAcceleration;
    float restitution;
public:
    Bouncer(Mesh* me, Material* ma) : MeshInstance(me,ma){
        isEnemy = true;
        position = float3(0,0,0);
        restitution = 1;
        angularVelocity = 1;
        velocity = float3().random()*50;
        velocity.y = 0;
    
    }
    
    virtual void move(double t, double dt){
        position += velocity*dt;
        velocity += float3(0,-10,0) * dt;
        orientationAngle += angularVelocity*dt;
        
        
        if(position.y < 0){
            velocity.y *= -restitution;
            position.y = 0;
        }
        angularVelocity *= pow(0.8, dt);
        
    }
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects,std::vector<Mesh*>& meshs, std::vector<Material*>& materials)
    {
        
        if(position.x > dimension){
            position.x = dimension;
            velocity.x *= -.5;
        }
        if(position.x < -dimension){
            position.x = -dimension;
            velocity.x *= -.5;
        }
        if(position.z > dimension){
            position.z = dimension;
            velocity.z *= -.5;
        }
        if(position.z < -dimension){
            position.z = -dimension;
            velocity.z *= -.5;
        }

        
        return false;
    }
    

};

class Ground : public Object{
public:
    Ground(Material* m) : Object(m){}
    
    void draw()
    {
        glDisable(GL_LIGHTING);
        glColor3f(0,.8,0);
        material->apply();
        // apply scaling, translation and orientation
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
        glPopMatrix();
        glEnable(GL_LIGHTING);
    }
    
    void drawModel(){
        
        glBegin(GL_TRIANGLE_STRIP);
        glVertex4f(-dimension, 0, -dimension, 1);
        glVertex4f(-dimension, 0, dimension, 1);
        glVertex4f(dimension, 0, -dimension, 1);
        glVertex4f(dimension, 0, dimension, 1);
        glEnd();
        
    
    }
    
    
    virtual void drawShadow(float3 lightDir){}
};

class Scene
{
    Camera camera;
    bool newGame = false;
    std::vector<LightSource*> lightSources;
    std::vector<Object*> objects;
    std::vector<Material*> materials;
    std::vector<Mesh*> meshs;
    std::vector<Billboard*> billboards;
public:
    void initialize()
    {
        newGame = false;
        score = 0;
        restart();
        
        TexturedMaterial* bullet = new TexturedMaterial("meshpack/bullet2.png");
        
        Billboard* b = new Billboard(bullet);
        billboards.push_back(b);
        
        // BUILD YOUR SCENE HERE
        Mesh* chevy = new Mesh("meshpack/chevy/chevy.obj");
        TexturedMaterial* chevySkin = new TexturedMaterial("meshpack/chevy/chevy.png");
        
        Mesh* tigger = new Mesh("meshpack/tigger.obj");
        TexturedMaterial* tigskin = new TexturedMaterial("meshpack/tigger.png");
        
        Avatar* car = new Avatar(chevy, chevySkin);
        car->scale(float3(.25, .25, .25));
        objects.push_back(car);
        
        Mesh* tree = new Mesh("meshpack/tree/smoothtree.obj");
        TexturedMaterial* treeskin = new TexturedMaterial("meshpack/tree/tree.png");
        
        meshs.push_back(tigger);
        materials.push_back(tigskin);
        meshs.push_back(tree);
        materials.push_back(treeskin);
        
        lightSources.push_back(
                               new DirectionalLight(
                                                    float3(0, 1, 0),
                                                    float3(1, 0.5, 1)));
        lightSources.push_back(
                               new PointLight(
                                              float3(-1, -1, 1),
                                              float3(0.2, 0.1, 0.1)));
        Material* yellowDiffuseMaterial = new Material();
        materials.push_back(yellowDiffuseMaterial);
        yellowDiffuseMaterial->kd = float3(1, 1, 0);
        
        Material* greenDiffuseMaterial = new Material();
        materials.push_back(greenDiffuseMaterial);
        greenDiffuseMaterial->kd = float3(0, 1, 0);
        
        
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        
        
        
        /*CREATE ENVIRONMENT*/
        for(int i = 0; i < 10; i++){
            Tree* t = new Tree(tree,treeskin);
            float2 randomLoc = float2().random()*dimension;
            float3 random;
            random.x = randomLoc.x;
            random.y = 0;
            random.z = randomLoc.y;
            t->translate(random);
            objects.push_back(t);
        
        }
        
        
        /*CREATE TEAPOT*/
        Teapot* start = new Teapot(yellowDiffuseMaterial);
        float3 random = float3().random()*dimension;
        random.y = 0;
        start->translate(random);
        objects.push_back(new Teapot(yellowDiffuseMaterial));
        
        
        /*CREATE THE GROUND*/
        Ground* theGround = new Ground(greenDiffuseMaterial);
        objects.push_back(theGround);
        
        
        
        
    }
    void restart(){
        for (int i = 0; i < lightSources.size(); i++){
            LightSource* ls = lightSources.at(i);
            lightSources.erase(lightSources.begin() + i);
            i--;
            delete ls;
        }
        for (int i = 0; i < materials.size(); i++){
            Material* m = materials.at(i);
            materials.erase(materials.begin() + i);
            i--;
            delete m;
        }
        for (int i = 0; i < objects.size(); i++){
            Object* o = objects.at(i);
            objects.erase(objects.begin() + i);
            i--;
            delete o;
        }
        for (int i = 0; i < meshs.size(); i++){
            Mesh* m = meshs.at(i);
            meshs.erase(meshs.begin() + i);
            i--;
            delete m;
        }
        for (int i = 0; i < billboards.size(); i++){
            Billboard* b = billboards.at(i);
            billboards.erase(billboards.begin() + i);
            i--;
            delete b;
        }
    }
    
    bool getNewGame(){
        return newGame;
    }
    
    void move(std::vector<bool> keysPressed, double t, double dt) {
        Object* avatar = objects.at(0);
        getCamera().move(avatar->getPosition(),avatar->getOrientation(), dt, keysPressed);
        billboards.at(0)->setPosition(avatar->getPosition());
        for(int i=0; i<objects.size(); i++){
            objects.at(i)->move(t, dt);
        }
    }
    
    void control(std::vector<bool> keysPressed) {
        std::vector<Object*> spawn;
        for(int i=0; i<objects.size(); i++){
            objects.at(i)->control(keysPressed, spawn, objects, meshs,materials);
            if(objects.at(i)->getIsDead() && !objects.at(i)->getIsAvatar() && !objects.at(i)->getIsEnemy()){
                Object* dead = objects.at(i);
                objects.erase(objects.begin() + i);
                i--;
                delete dead;
                
            }
            
            if(objects.at(i)->getIsDead() && objects.at(i)->getIsAvatar()){
                newGame = true;
            }

        }
        
        for(int i = 0; i < spawn.size(); i++)
            objects.push_back(spawn.at(i));
    }
    
    ~Scene()
    {
        for (std::vector<LightSource*>::iterator iLightSource = lightSources.begin(); iLightSource != lightSources.end(); ++iLightSource)
            delete *iLightSource;
        for (std::vector<Material*>::iterator iMaterial = materials.begin(); iMaterial != materials.end(); ++iMaterial)
            delete *iMaterial;
        for (std::vector<Object*>::iterator iObject = objects.begin(); iObject != objects.end(); ++iObject)
            delete *iObject;
        for (std::vector<Mesh*>::iterator iMesh = meshs.begin(); iMesh != meshs.end(); ++iMesh)
            delete *iMesh;
        for (std::vector<Billboard*>::iterator iBillboard = billboards.begin(); iBillboard != billboards.end(); ++iBillboard)
            delete *iBillboard;
    }
    
    
    
public:
    Camera& getCamera()
    {
        return camera;
    }
    
    void draw()
    {
        camera.apply();
        unsigned int iLightSource=0;
        for (; iLightSource<lightSources.size(); iLightSource++)
        {
            glEnable(GL_LIGHT0 + iLightSource);
            lightSources.at(iLightSource)->apply(GL_LIGHT0 + iLightSource);
        }
        for (; iLightSource<GL_MAX_LIGHTS; iLightSource++)
            glDisable(GL_LIGHT0 + iLightSource);
        
        for (unsigned int iObject=0; iObject<objects.size(); iObject++){
            objects.at(iObject)->drawShadow(float3(0,1,0));
            objects.at(iObject)->draw();
        }
        for (unsigned int iBillboard=0; iBillboard<billboards.size(); iBillboard++){
            billboards.at(iBillboard)->draw(this->getCamera());
        }
    }
};

Scene scene;
std::vector<bool> keysPressed;


void onDisplay( ) {
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear screen
    
    
    if(scene.getNewGame())
        scene.initialize();
    scene.draw();
    sc.draw(scene.getCamera());
    
    glutSwapBuffers(); // drawing finished
}



void onIdle()
{
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;        	// time elapsed since starting this program in msec
    static double lastTime = 0.0;
    double dt = t - lastTime;
    lastTime = t;
    
    
    scene.move(keysPressed,t,dt);
    scene.control(keysPressed);
    
    glutPostRedisplay();
}

void onKeyboard(unsigned char key, int x, int y)
{
    keysPressed.at(key) = true;
}

void onKeyboardUp(unsigned char key, int x, int y)
{
    keysPressed.at(key) = false;
}

void onMouse(int button, int state, int x, int y)
{
    if(button == GLUT_LEFT_BUTTON)
        if(state == GLUT_DOWN)
            scene.getCamera().startDrag(x, y);
        else
            scene.getCamera().endDrag();
}

void onMouseMotion(int x, int y)
{
    scene.getCamera().drag(x, y);
}

void onReshape(int winWidth, int winHeight)
{
    glViewport(0, 0, winWidth, winHeight);
    scene.getCamera().setAspectRatio((float)winWidth/winHeight);
}	

int main(int argc, char **argv) {
    glutInit(&argc, argv);						// initialize GLUT
    glutInitWindowSize(600, 600);				// startup window size 
    glutInitWindowPosition(100, 100);           // where to put window on screen
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);    // 8 bit R,G,B,A + double buffer + depth buffer
    
    glutCreateWindow("OpenGL teapots");				// application window is created and displayed
    
    glViewport(0, 0, 600, 600);
    
    glutDisplayFunc(onDisplay);					// register callback
    glutIdleFunc(onIdle);						// register callback
    glutReshapeFunc(onReshape);
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMouseMotion);
    
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    
    scene.initialize();
    for(int i=0; i<256; i++)
        keysPressed.push_back(false);
    
    glutMainLoop();								// launch event handling loop
    
    return 0;
}