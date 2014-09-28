/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#ifndef _Ogre_GL3PlusVaoManager_H_
#define _Ogre_GL3PlusVaoManager_H_

#include "OgreGL3PlusPrerequisites.h"
#include "Vao/OgreVaoManager.h"

namespace Ogre
{
    class _OgreGL3PlusExport GL3PlusVaoManager : public VaoManager
    {
    protected:
        enum VboFlag
        {
            CPU_INACCESSIBLE,
            CPU_ACCESSIBLE,
            MAX_VBO_FLAG
        };

    public:
        struct Block
        {
            size_t offset;
            size_t size;

            Block( size_t _offset, size_t _size ) : offset( _offset ), size( _size ) {}
        };
        struct StrideChanger
        {
            size_t offsetAfterPadding;
            size_t paddedBytes;

            StrideChanger() : offsetAfterPadding( 0 ), paddedBytes( 0 ) {}
            StrideChanger( size_t _offsetAfterPadding, size_t _paddedBytes ) :
                offsetAfterPadding( _offsetAfterPadding ), paddedBytes( _paddedBytes ) {}

            bool operator () ( const StrideChanger &left, size_t right ) const
            {
                return left.offsetAfterPadding < right;
            }
            bool operator () ( size_t left, const StrideChanger &right ) const
            {
                return left < right.offsetAfterPadding;
            }
            bool operator () ( const StrideChanger &left, const StrideChanger &right ) const
            {
                return left.offsetAfterPadding < right.offsetAfterPadding;
            }
        };

        typedef vector<Block>::type BlockVec;
        typedef vector<StrideChanger>::type StrideChangerVec;

    protected:
        struct Vbo
        {
            GLuint vboName;
            size_t sizeBytes;

            BlockVec            freeBlocks;
            StrideChangerVec    strideChangers;
        };

        struct Vao
        {
            GLuint vaoName;

            struct VertexBinding
            {
                GLuint              vertexBufferVbo;
                VertexElement2Vec   vertexElements;
                GLsizei             stride;
                size_t              offset;

                //OpenGL supports this parameter per attribute, but
                //we're a bit more conservative and do it per buffer
                GLuint              instancingDivisor;

                bool operator == ( const VertexBinding &_r ) const
                {
                    return vertexBufferVbo == _r.vertexBufferVbo &&
                            vertexElements == _r.vertexElements &&
                            stride == _r.stride &&
                            offset == _r.offset &&
                            instancingDivisor == _r.instancingDivisor;
                }
            };

            typedef vector<VertexBinding>::type VertexBindingVec;

            VertexBindingVec    vertexBuffers;
            GLuint              indexBufferVbo;
            IndexBufferPacked::IndexType indexType;
            uint32              refCount;
        };

        typedef vector<Vbo>::type VboVec;
        typedef vector<Vao>::type VaoVec;
        typedef map<VertexElement2Vec, Vbo>::type VboMap;
        typedef vector<GLsync>::type GLSyncVec;

        VboVec  mVbos[MAX_VBO_FLAG];
        /// MultiSource VBOs request a block from mVbo (i.e. they call allocateVbo) and thus do not
        /// own the vboName. For the rest, the way they manage free blocks is almost the same as
        /// with regular mVbos.
        VboMap  mMultiSourceVbos[MAX_VBO_FLAG];
        size_t  mDefaultPoolSize[MAX_VBO_FLAG];

        VaoVec  mVaos;

        GLSyncVec mFrameSyncVec;

        /// True if ARB_buffer_storage is supported (Persistent Mapping and immutable buffers)
        bool    mArbBufferStorage;

        /** Asks for allocating buffer space in a VBO (Vertex Buffer Object).
            If the VBO doesn't exist, all VBOs are full or can't fit this request,
            then a new VBO will be created.
        @remarks
            Can throw if out of video memory
        @param sizeBytes
            The requested size, in bytes.
        @param bytesPerElement
            The number of bytes per vertex or per index (i.e. 16-bit indices = 2).
            Cannot be 0.
        @param bufferType
            The type of buffer
        @param outVboIdx [out]
            The index to the mVbos.
        @param outBufferOffset [out]
            The offset in bytes at which the buffer data should be placed.
        */
        void allocateVbo( size_t sizeBytes, size_t alignment, BufferType bufferType,
                          size_t &outVboIdx, size_t &outBufferOffset );

        /** Deallocates a buffer allocated with @allocateVbo.
        @remarks
            All four parameters *must* match with the ones provided to or
            returned from allocateVbo, otherwise the behavior is undefined.
        @param vboIdx
            The index to the mVbos pool that was returned by allocateVbo
        @param bufferOffset
            The buffer offset that was returned by allocateVbo
        @param sizeBytes
            The sizeBytes parameter that was passed to allocateVbos.
        @param bufferType
            The type of buffer that was passed to allocateVbo.
        */
        void deallocateVbo( size_t vboIdx, size_t bufferOffset, size_t sizeBytes,
                            BufferType bufferType );

    public:
        /** Starts with "blockToMerge" and recursively merges all blocks which
            end up being affected by "blockToMerge".
        @remarks
            Example: if "blocks" contains C, A, E, B and blockToMerge is B,
            then A will be merged with B, then AB will be merged with C
            So now "blocks" contains ABC and E.
        @param blockToMerge
            Iterator to a block to merge. Must belong to "blocks". Iterator may
            be invalidated after calling this function.
        @param blocks
            Vector of blocks where blockToMerge belongs to.
        */
        static void mergeContiguousBlocks( BlockVec::iterator blockToMerge,
                                           BlockVec &blocks );

    protected:
        virtual VertexBufferPacked* createVertexBufferImpl( size_t numElements,
                                                            uint32 bytesPerElement,
                                                            BufferType bufferType,
                                                            void *initialData, bool keepAsShadow,
                                                            const VertexElement2Vec &vertexElements );

        virtual void destroyVertexBufferImpl( VertexBufferPacked *vertexBuffer );

        virtual MultiSourceVertexBufferPool* createMultiSourceVertexBufferPoolImpl(
                                            const VertexElement2VecVec &vertexElementsBySource,
                                            size_t maxNumVertices, size_t totalBytesPerVertex,
                                            BufferType bufferType );

        virtual IndexBufferPacked* createIndexBufferImpl( size_t numElements,
                                                          uint32 bytesPerElement,
                                                          BufferType bufferType,
                                                          void *initialData, bool keepAsShadow );

        virtual void destroyIndexBufferImpl( IndexBufferPacked *indexBuffer );

        virtual ConstBufferPacked* createConstBufferImpl( size_t sizeBytes, BufferType bufferType,
                                                          void *initialData, bool keepAsShadow );
        virtual void destroyConstBufferImpl( ConstBufferPacked *constBuffer );

        virtual TexBufferPacked* createTexBufferImpl( PixelFormat pixelFormat, size_t sizeBytes,
                                                      BufferType bufferType,
                                                      void *initialData, bool keepAsShadow );
        virtual void destroyTexBufferImpl( TexBufferPacked *texBuffer );

        virtual IndirectBufferPacked* createIndirectBufferImpl( size_t sizeBytes, BufferType bufferType,
                                                                void *initialData, bool keepAsShadow );
        virtual void destroyIndirectBufferImpl( IndirectBufferPacked *indirectBuffer );

        GLuint createVao( const Vao &vaoRef );

        virtual VertexArrayObject* createVertexArrayObjectImpl(
                                                        const VertexBufferPackedVec &vertexBuffers,
                                                        IndexBufferPacked *indexBuffer,
                                                        v1::RenderOperation::OperationType opType );

        virtual void destroyVertexArrayObjectImpl( VertexArrayObject *vao );

    public:
        GL3PlusVaoManager( bool supportsArbBufferStorage, bool supportsIndirectBuffers );
        virtual ~GL3PlusVaoManager();

        bool supportsArbBufferStorage(void) const       { return mArbBufferStorage; }

        /** Creates a new staging buffer and adds it to the pool. @see getStagingBuffer.
        @remarks
            The returned buffer starts with a reference count of 1. You should decrease
            it when you're done using it.
        */
        virtual StagingBuffer* createStagingBuffer( size_t sizeBytes, bool forUpload );

        virtual void _update(void);

        /// Returns the current frame # (which wraps to 0 every mDynamicBufferMultiplier
        /// times). But first stalls until that mDynamicBufferMultiplier-1 frame behind
        /// is finished.
        uint8 waitForTailFrameToFinish(void);
    };
}

#endif