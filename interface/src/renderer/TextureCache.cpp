//
//  TextureCache.cpp
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 8/6/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// include this before QGLWidget, which includes an earlier version of OpenGL
#include "InterfaceConfig.h"

#include <QGLWidget>
#include <QNetworkReply>
#include <QOpenGLFramebufferObject>
#include <QRunnable>
#include <QThreadPool>

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

#include "Application.h"
#include "TextureCache.h"

TextureCache::TextureCache() :
    _permutationNormalTextureID(0),
    _whiteTextureID(0),
    _blueTextureID(0),
    _primaryDepthTextureID(0),
    _primaryFramebufferObject(NULL),
    _secondaryFramebufferObject(NULL),
    _tertiaryFramebufferObject(NULL),
    _shadowFramebufferObject(NULL),
    _frameBufferSize(100, 100)
{
}

TextureCache::~TextureCache() {
    if (_permutationNormalTextureID != 0) {
        glDeleteTextures(1, &_permutationNormalTextureID);
    }
    if (_whiteTextureID != 0) {
        glDeleteTextures(1, &_whiteTextureID);
    }
    if (_primaryFramebufferObject) {
        glDeleteTextures(1, &_primaryDepthTextureID);
    }
    
    if (_primaryFramebufferObject) {
        delete _primaryFramebufferObject;
    }

    if (_secondaryFramebufferObject) {
        delete _secondaryFramebufferObject;
    }

    if (_tertiaryFramebufferObject) {
        delete _tertiaryFramebufferObject;
    }
}

void TextureCache::setFrameBufferSize(QSize frameBufferSize) {
    //If the size changed, we need to delete our FBOs
    if (_frameBufferSize != frameBufferSize) {
        _frameBufferSize = frameBufferSize;

        if (_primaryFramebufferObject) {
            delete _primaryFramebufferObject;
            _primaryFramebufferObject = NULL;
            glDeleteTextures(1, &_primaryDepthTextureID);
            _primaryDepthTextureID = 0;
        }

        if (_secondaryFramebufferObject) {
            delete _secondaryFramebufferObject;
            _secondaryFramebufferObject = NULL;
        }

        if (_tertiaryFramebufferObject) {
            delete _tertiaryFramebufferObject;
            _tertiaryFramebufferObject = NULL;
        }
    }
}

GLuint TextureCache::getPermutationNormalTextureID() {
    if (_permutationNormalTextureID == 0) {
        glGenTextures(1, &_permutationNormalTextureID);
        glBindTexture(GL_TEXTURE_2D, _permutationNormalTextureID);
        
        // the first line consists of random permutation offsets
        unsigned char data[256 * 2 * 3];
        for (int i = 0; i < 256 * 3; i++) {
            data[i] = rand() % 256;
        }
        // the next, random unit normals
        for (int i = 256 * 3; i < 256 * 3 * 2; i += 3) {
            glm::vec3 randvec = glm::sphericalRand(1.0f);
            data[i] = ((randvec.x + 1.0f) / 2.0f) * 255.0f;
            data[i + 1] = ((randvec.y + 1.0f) / 2.0f) * 255.0f;
            data[i + 2] = ((randvec.z + 1.0f) / 2.0f) * 255.0f;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return _permutationNormalTextureID;
}

const unsigned char OPAQUE_WHITE[] = { 0xFF, 0xFF, 0xFF, 0xFF };
const unsigned char OPAQUE_BLUE[] = { 0x80, 0x80, 0xFF, 0xFF };

static void loadSingleColorTexture(const unsigned char* color) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

GLuint TextureCache::getWhiteTextureID() {
    if (_whiteTextureID == 0) {
        glGenTextures(1, &_whiteTextureID);
        glBindTexture(GL_TEXTURE_2D, _whiteTextureID);
        loadSingleColorTexture(OPAQUE_WHITE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return _whiteTextureID;
}

GLuint TextureCache::getBlueTextureID() {
    if (_blueTextureID == 0) {
        glGenTextures(1, &_blueTextureID);
        glBindTexture(GL_TEXTURE_2D, _blueTextureID);
        loadSingleColorTexture(OPAQUE_BLUE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return _blueTextureID;
}

/// Extra data for creating textures.
class TextureExtra {
public:
    bool normalMap;
    const QByteArray& content;
};

QSharedPointer<NetworkTexture> TextureCache::getTexture(const QUrl& url, bool normalMap,
        bool dilatable, const QByteArray& content) {
    if (!dilatable) {
        TextureExtra extra = { normalMap, content };
        return ResourceCache::getResource(url, QUrl(), false, &extra).staticCast<NetworkTexture>();
    }
    QSharedPointer<NetworkTexture> texture = _dilatableNetworkTextures.value(url);
    if (texture.isNull()) {
        texture = QSharedPointer<NetworkTexture>(new DilatableNetworkTexture(url, content), &Resource::allReferencesCleared);
        texture->setSelf(texture);
        texture->setCache(this);
        _dilatableNetworkTextures.insert(url, texture);
    } else {
        _unusedResources.remove(texture->getLRUKey());
    }
    return texture;
}

QOpenGLFramebufferObject* TextureCache::getPrimaryFramebufferObject() {

    if (!_primaryFramebufferObject) {
        _primaryFramebufferObject = createFramebufferObject();
       
        glGenTextures(1, &_primaryDepthTextureID);
        glBindTexture(GL_TEXTURE_2D, _primaryDepthTextureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, _frameBufferSize.width(), _frameBufferSize.height(),
            0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        _primaryFramebufferObject->bind();
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _primaryDepthTextureID, 0);
        _primaryFramebufferObject->release();
    }
    return _primaryFramebufferObject;
}

GLuint TextureCache::getPrimaryDepthTextureID() {
    // ensure that the primary framebuffer object is initialized before returning the depth texture id
    getPrimaryFramebufferObject();
    return _primaryDepthTextureID;
}

QOpenGLFramebufferObject* TextureCache::getSecondaryFramebufferObject() {
    if (!_secondaryFramebufferObject) {
        _secondaryFramebufferObject = createFramebufferObject();
    }
    return _secondaryFramebufferObject;
}

QOpenGLFramebufferObject* TextureCache::getTertiaryFramebufferObject() {
    if (!_tertiaryFramebufferObject) {
        _tertiaryFramebufferObject = createFramebufferObject();
    }
    return _tertiaryFramebufferObject;
}

QOpenGLFramebufferObject* TextureCache::getShadowFramebufferObject() {
    if (!_shadowFramebufferObject) {
        const int SHADOW_MAP_SIZE = 2048;
        _shadowFramebufferObject = new QOpenGLFramebufferObject(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
            QOpenGLFramebufferObject::NoAttachment, GL_TEXTURE_2D, GL_RGB);
        
        glGenTextures(1, &_shadowDepthTextureID);
        glBindTexture(GL_TEXTURE_2D, _shadowDepthTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
            0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float DISTANT_BORDER[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, DISTANT_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        _shadowFramebufferObject->bind();
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _shadowDepthTextureID, 0);
        _shadowFramebufferObject->release();
    }
    return _shadowFramebufferObject;
}

GLuint TextureCache::getShadowDepthTextureID() {
    // ensure that the shadow framebuffer object is initialized before returning the depth texture id
    getShadowFramebufferObject();
    return _shadowDepthTextureID;
}

bool TextureCache::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize) {
        QSize size = static_cast<QResizeEvent*>(event)->size();
        if (_primaryFramebufferObject && _primaryFramebufferObject->size() != size) {
            delete _primaryFramebufferObject;
            _primaryFramebufferObject = NULL;
            glDeleteTextures(1, &_primaryDepthTextureID);
        }
        if (_secondaryFramebufferObject && _secondaryFramebufferObject->size() != size) {
            delete _secondaryFramebufferObject;
            _secondaryFramebufferObject = NULL;
        }
        if (_tertiaryFramebufferObject && _tertiaryFramebufferObject->size() != size) {
            delete _tertiaryFramebufferObject;
            _tertiaryFramebufferObject = NULL;
        }
    }
    return false;
}

QSharedPointer<Resource> TextureCache::createResource(const QUrl& url,
        const QSharedPointer<Resource>& fallback, bool delayLoad, const void* extra) {
    const TextureExtra* textureExtra = static_cast<const TextureExtra*>(extra);
    return QSharedPointer<Resource>(new NetworkTexture(url, textureExtra->normalMap, textureExtra->content),
        &Resource::allReferencesCleared);
}

QOpenGLFramebufferObject* TextureCache::createFramebufferObject() {
    QOpenGLFramebufferObject* fbo = new QOpenGLFramebufferObject(_frameBufferSize);
    Application::getInstance()->getGLWidget()->installEventFilter(this);
    
    glBindTexture(GL_TEXTURE_2D, fbo->texture());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return fbo;
}

Texture::Texture() {
    glGenTextures(1, &_id);
}

Texture::~Texture() {
    glDeleteTextures(1, &_id);
}

NetworkTexture::NetworkTexture(const QUrl& url, bool normalMap, const QByteArray& content) :
    Resource(url, !content.isEmpty()),
    _translucent(false) {
    
    if (!url.isValid()) {
        _loaded = true;
    }
    
    // default to white/blue
    glBindTexture(GL_TEXTURE_2D, getID());
    loadSingleColorTexture(normalMap ? OPAQUE_BLUE : OPAQUE_WHITE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // if we have content, load it after we have our self pointer
    if (!content.isEmpty()) {
        _startedLoading = true;
        QMetaObject::invokeMethod(this, "loadContent", Qt::QueuedConnection, Q_ARG(const QByteArray&, content));
    }
}

class ImageReader : public QRunnable {
public:

    ImageReader(const QWeakPointer<Resource>& texture, QNetworkReply* reply, const QUrl& url = QUrl(),
        const QByteArray& content = QByteArray());
    
    virtual void run();

private:
    
    QWeakPointer<Resource> _texture;
    QNetworkReply* _reply;
    QUrl _url;
    QByteArray _content;
};

ImageReader::ImageReader(const QWeakPointer<Resource>& texture, QNetworkReply* reply,
        const QUrl& url, const QByteArray& content) :
    _texture(texture),
    _reply(reply),
    _url(url),
    _content(content) {
}

void ImageReader::run() {
    QSharedPointer<Resource> texture = _texture.toStrongRef();
    if (texture.isNull()) {
        if (_reply) {
            _reply->deleteLater();
        }
        return;
    }
    if (_reply) {
        _url = _reply->url();
        _content = _reply->readAll();
        _reply->deleteLater();
    }
    QImage image = QImage::fromData(_content);
    
    // enforce a fixed maximum
    const int MAXIMUM_SIZE = 1024;
    if (image.width() > MAXIMUM_SIZE || image.height() > MAXIMUM_SIZE) {
        qDebug() << "Image greater than maximum size:" << _url << image.width() << image.height();
        image = image.scaled(MAXIMUM_SIZE, MAXIMUM_SIZE, Qt::KeepAspectRatio);
    }
    
    if (!image.hasAlphaChannel()) {
        if (image.format() != QImage::Format_RGB888) {
            image = image.convertToFormat(QImage::Format_RGB888);
        }
        QMetaObject::invokeMethod(texture.data(), "setImage", Q_ARG(const QImage&, image), Q_ARG(bool, false));
        return;
    }
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }
    
    // check for translucency/false transparency
    int opaquePixels = 0;
    int translucentPixels = 0;
    const int EIGHT_BIT_MAXIMUM = 255;
    const int RGB_BITS = 24;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            int alpha = image.pixel(x, y) >> RGB_BITS;
            if (alpha == EIGHT_BIT_MAXIMUM) {
                opaquePixels++;
            } else if (alpha != 0) {
                translucentPixels++;
            }
        }
    }
    int imageArea = image.width() * image.height();
    if (opaquePixels == imageArea) {
        qDebug() << "Image with alpha channel is completely opaque:" << _url;
        image = image.convertToFormat(QImage::Format_RGB888);
    }
    QMetaObject::invokeMethod(texture.data(), "setImage", Q_ARG(const QImage&, image),
        Q_ARG(bool, translucentPixels >= imageArea / 2));
}

void NetworkTexture::downloadFinished(QNetworkReply* reply) {
    // send the reader off to the thread pool
    QThreadPool::globalInstance()->start(new ImageReader(_self, reply));
}

void NetworkTexture::loadContent(const QByteArray& content) {
    QThreadPool::globalInstance()->start(new ImageReader(_self, NULL, _url, content));
}

void NetworkTexture::setImage(const QImage& image, bool translucent) {
    _translucent = translucent;
    
    finishedLoading(true);
    imageLoaded(image);
    glBindTexture(GL_TEXTURE_2D, getID());
    if (image.hasAlphaChannel()) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0,
            GL_BGRA, GL_UNSIGNED_BYTE, image.constBits());
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width(), image.height(), 0,
            GL_RGB, GL_UNSIGNED_BYTE, image.constBits());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void NetworkTexture::imageLoaded(const QImage& image) {
    // nothing by default
}

DilatableNetworkTexture::DilatableNetworkTexture(const QUrl& url, const QByteArray& content) :
    NetworkTexture(url, false, content),
    _innerRadius(0),
    _outerRadius(0)
{
}

QSharedPointer<Texture> DilatableNetworkTexture::getDilatedTexture(float dilation) {
    QSharedPointer<Texture> texture = _dilatedTextures.value(dilation);
    if (texture.isNull()) {
        texture = QSharedPointer<Texture>(new Texture());
        
        if (!_image.isNull()) {
            QImage dilatedImage = _image;
            QPainter painter;
            painter.begin(&dilatedImage);
            QPainterPath path;
            qreal radius = glm::mix((float) _innerRadius, (float) _outerRadius, dilation);
            path.addEllipse(QPointF(_image.width() / 2.0, _image.height() / 2.0), radius, radius);
            painter.fillPath(path, Qt::black);
            painter.end();
            
            glBindTexture(GL_TEXTURE_2D, texture->getID());
            if (dilatedImage.hasAlphaChannel()) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dilatedImage.width(), dilatedImage.height(), 0,
                    GL_BGRA, GL_UNSIGNED_BYTE, dilatedImage.constBits());
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dilatedImage.width(), dilatedImage.height(), 0,
                    GL_RGB, GL_UNSIGNED_BYTE, dilatedImage.constBits());
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
        _dilatedTextures.insert(dilation, texture);
    }
    return texture;
}

void DilatableNetworkTexture::imageLoaded(const QImage& image) {
    _image = image;
    
    // scan out from the center to find inner and outer radii
    int halfWidth = image.width() / 2;
    int halfHeight = image.height() / 2;
    const int BLACK_THRESHOLD = 32;
    while (_innerRadius < halfWidth && qGray(image.pixel(halfWidth + _innerRadius, halfHeight)) < BLACK_THRESHOLD) {
        _innerRadius++;
    }
    _outerRadius = _innerRadius;
    const int TRANSPARENT_THRESHOLD = 32;
    while (_outerRadius < halfWidth && qAlpha(image.pixel(halfWidth + _outerRadius, halfHeight)) > TRANSPARENT_THRESHOLD) {
        _outerRadius++;
    }
    
    // clear out any textures we generated before loading
    _dilatedTextures.clear();
}

void DilatableNetworkTexture::reinsert() {
    static_cast<TextureCache*>(_cache.data())->_dilatableNetworkTextures.insert(_url,
        qWeakPointerCast<NetworkTexture, Resource>(_self));    
}

