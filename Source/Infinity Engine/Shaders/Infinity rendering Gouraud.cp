/*****************************************************************************
 * Copyright (c) 1998-2001, French Touch, SARL
 * http://www.french-touch.net
 * info@french-touch.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


#include				<fp.h>

#include				"Infinity Structures.h"
#include				"Matrix.h"
#include				"Vector.h"
#include				"Infinity rendering.h"
#include				"Infinity Utils.h"
#include				"Clipping Utils.h"
#include				"Projection Utils.h"

//ROUTINES:

void Gouraud_DrawShape_NoShading(StatePtr state, ShapePtr shape, MatrixPtr globalPos, MatrixPtr negatedCamera, VectorPtr cameraPos, ShapePtr shapeList[], long clip)
{
	long				x;
	Matrix				r1,
						localPos;
	TQAIndexedTriangle	*trianglePtr;
	TQAVGouraud			*texturePtr;
	long				triangleCount,
						triangleCount2,
						verticeCount;
	VertexPtr			vertexPtr;
	TriFacePtr			triPtr;
	Vector				v1,
						v2,
						n;
	
	//Get local matrix - max depth = 4
	localPos = *globalPos;
	if(shape->parent != nil)
	Shape_LinkMatrixByParent(shape, &localPos);
	if(shape->flags & kFlag_RelativePos)
	Matrix_Cat(&shape->pos, &localPos, &localPos);
	Matrix_Cat(&localPos, negatedCamera, &r1);
	
	//Test visibility
	if(clip == kUndetermined) {
		clip = Box_Visible(&r1, shape->boundingBox, state);
		if(clip == kNotVisible)
		return;
	}
	
	//Create vertice buffer and trianglesBuffer buffer
	verticeCount = shape->pointCount;
	triangleCount = 0;
	triangleCount2 = 0;
	
	//Fill vertice buffer
	texturePtr = vGouraudBuffer;
	vertexPtr = shape->pointList;
	for(x = 0; x < shape->pointCount; ++x) {
		//Calculate vertex coordinates
		Matrix_TransformVector(&r1, &vertexPtr->point, (VectorPtr) texturePtr);
		
		//Fill vertex
		texturePtr->r = vertexPtr->u;
		texturePtr->g = vertexPtr->v;
		texturePtr->b = vertexPtr->c;
		texturePtr->a = shape->alpha;
		
		++texturePtr;
		++vertexPtr;
	}
	
	//Fill trianglesBuffer buffer #1
	triPtr = shape->triangleList;
	for(x = 0; x < shape->triangleCount; ++x) {
		//Remove backfacing trianglesBuffer
		if(shape->backfaceCulling) {
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[1]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v1);
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[2]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v2);
			Vector_CrossProduct(&v2, &v1, &n);
			if(Vector_DotProduct((VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &n) > 0) {
				++triPtr;
				continue;
			}
		}

		//Clip triangle Z
		if(clip & kZClipping)
		ClipTriangleGouraud_Z(state, vGouraudBuffer, &verticeCount, trianglesBuffer, &triangleCount, triPtr->corner[0], triPtr->corner[1], triPtr->corner[2]);
		else {
			trianglesBuffer[triangleCount].triangleFlags = 0;
			trianglesBuffer[triangleCount].vertices[0] = triPtr->corner[0];
			trianglesBuffer[triangleCount].vertices[1] = triPtr->corner[1];
			trianglesBuffer[triangleCount].vertices[2] = triPtr->corner[2];
			++triangleCount;
		}
		
		++triPtr;
	}
	
	//Project vGouraudBuffer
	Project_GouraudBuffer(state, verticeCount);
	
	if(clip & kXYClipping) {
		//Clip trianglesBuffer and fill triangle buffer #2
		trianglePtr = trianglesBuffer;
		for(x = 0; x < triangleCount; ++x) {
			ClipTriangleGouraud_XY(state, vGouraudBuffer, &verticeCount, trianglesBuffer2, &triangleCount2, trianglePtr->vertices[0], trianglePtr->vertices[1], trianglePtr->vertices[2]);
			++trianglePtr;
		}
		
		//Draw trianglesBuffer
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount2, trianglesBuffer2);
	}
	else {
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount, trianglesBuffer);
	}

	//Flush!
	//QAFlush(state->drawContext);
}

void Gouraud_DrawShape_LambertShading(StatePtr state, ShapePtr shape, MatrixPtr globalPos, MatrixPtr negatedCamera, VectorPtr cameraPos, ShapePtr shapeList[], long clip)
{
	long				x;
	Matrix				r1,
						r2,
						localPos;
	float				difuse,
						shapeDifuse = shape->difuse;
	TQAIndexedTriangle	*trianglePtr;
	TQAVGouraud			*texturePtr;
	long				triangleCount,
						triangleCount2,
						verticeCount;
	VertexPtr			vertexPtr;
	VectorPtr			normalPtr;
	TriFacePtr			triPtr;
	Vector				v1,
						v2,
						n,
						lightSource;
	float				l;
	
	//Get local matrix - max depth = 4
	localPos = *globalPos;
	if(shape->parent != nil)
	Shape_LinkMatrixByParent(shape, &localPos);
	if(shape->flags & kFlag_RelativePos)
	Matrix_Cat(&shape->pos, &localPos, &localPos);
	Matrix_Cat(&localPos, negatedCamera, &r1);
	
	//Test visibility
	if(clip == kUndetermined) {
		clip = Box_Visible(&r1, shape->boundingBox, state);
		if(clip == kNotVisible)
		return;
	}
	
	//Create vertice buffer and trianglesBuffer buffer
	verticeCount = shape->pointCount;
	triangleCount = 0;
	triangleCount2 = 0;
	
	//Build lightsource vector
	Matrix_Negate(&localPos, &r2);
	l = FVector_Length(state->lightVector);
	Matrix_RotateVector(&r2, &state->lightVector, &lightSource);
	Vector_Normalize(&lightSource, &lightSource);
	Vector_Multiply(l, &lightSource, &lightSource);
	
	//Fill vertice buffer
	texturePtr = vGouraudBuffer;
	vertexPtr = shape->pointList;
	normalPtr = shape->normalList;
	for(x = 0; x < shape->pointCount; ++x) {
		//Calculate diffuse
		difuse = Vector_DotProduct(&lightSource, normalPtr);
		//KEEP ONLY FOR EDITOR!! --------------------------------------------------------------
		if(difuse < 0) {
			if(shape->backfaceCulling)
			difuse = 0;
			else
			difuse = -difuse;
		}
		difuse = shapeDifuse * (state->ambient + difuse);
		if(difuse > 1.0)
		difuse = 1.0;

		//Calculate vertex coordinates
		Matrix_TransformVector(&r1, &vertexPtr->point, (VectorPtr) texturePtr);
		
		//Fill vertex
		texturePtr->r = vertexPtr->u * difuse;
		texturePtr->g = vertexPtr->v * difuse;
		texturePtr->b = vertexPtr->c * difuse;
		texturePtr->a = shape->alpha;
		
		++texturePtr;
		++vertexPtr;
		++normalPtr;
	}
	
	//Fill trianglesBuffer buffer #1
	triPtr = shape->triangleList;
	for(x = 0; x < shape->triangleCount; ++x) {
		//Remove backfacing trianglesBuffer
		if(shape->backfaceCulling) {
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[1]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v1);
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[2]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v2);
			Vector_CrossProduct(&v2, &v1, &n);
			if(Vector_DotProduct((VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &n) > 0) {
				++triPtr;
				continue;
			}
		}

		//Clip triangle Z
		if(clip & kZClipping)
		ClipTriangleGouraud_Z(state, vGouraudBuffer, &verticeCount, trianglesBuffer, &triangleCount, triPtr->corner[0], triPtr->corner[1], triPtr->corner[2]);
		else {
			trianglesBuffer[triangleCount].triangleFlags = 0;
			trianglesBuffer[triangleCount].vertices[0] = triPtr->corner[0];
			trianglesBuffer[triangleCount].vertices[1] = triPtr->corner[1];
			trianglesBuffer[triangleCount].vertices[2] = triPtr->corner[2];
			++triangleCount;
		}
		
		++triPtr;
	}
	
	//Project vGouraudBuffer
	Project_GouraudBuffer(state, verticeCount);
	
	if(clip & kXYClipping) {
		//Clip trianglesBuffer and fill triangle buffer #2
		trianglePtr = trianglesBuffer;
		for(x = 0; x < triangleCount; ++x) {
			ClipTriangleGouraud_XY(state, vGouraudBuffer, &verticeCount, trianglesBuffer2, &triangleCount2, trianglePtr->vertices[0], trianglePtr->vertices[1], trianglePtr->vertices[2]);
			++trianglePtr;
		}
		
		//Draw trianglesBuffer
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount2, trianglesBuffer2);
	}
	else {
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount, trianglesBuffer);
	}
	
	//Flush!
	//QAFlush(state->drawContext);
}

void Gouraud_DrawShape_PhongShading(StatePtr state, ShapePtr shape, MatrixPtr globalPos, MatrixPtr negatedCamera, VectorPtr cameraPos, ShapePtr shapeList[], long clip)
{
	long				x;
	Matrix				r1,
						r2,
						localPos;
	float				difuse,
						specular,
						shapeDifuse = shape->difuse,
						shapeSpecular = shape->specular;
	TQAIndexedTriangle	*trianglePtr;
	TQAVGouraud			*texturePtr;
	long				triangleCount,
						triangleCount2,
						verticeCount;
	VertexPtr			vertexPtr;
	VectorPtr			normalPtr;
	TriFacePtr			triPtr;
	Vector				v1,
						v2,
						n,
						lightSource,
						toCamera,
						reflectionVector;
	float				l;
						
	//Get local matrix - max depth = 4
	localPos = *globalPos;
	if(shape->parent != nil)
	Shape_LinkMatrixByParent(shape, &localPos);
	if(shape->flags & kFlag_RelativePos)
	Matrix_Cat(&shape->pos, &localPos, &localPos);
	Matrix_Cat(&localPos, negatedCamera, &r1);
	
	//Test visibility
	if(clip == kUndetermined) {
		clip = Box_Visible(&r1, shape->boundingBox, state);
		if(clip == kNotVisible)
		return;
	}
	
	//Create vertice buffer and trianglesBuffer buffer
	verticeCount = shape->pointCount;
	triangleCount = 0;
	triangleCount2 = 0;
	
	//Build lightsource vector
	Matrix_Negate(&localPos, &r2);
	l = FVector_Length(state->lightVector);
	Matrix_RotateVector(&r2, &state->lightVector, &lightSource);
	Vector_Normalize(&lightSource, &lightSource);
	Vector_Multiply(l, &lightSource, &lightSource);
	
	//Build to camera vector
	Vector_Subtract(cameraPos, &localPos.w, &toCamera);
	Matrix_RotateVector(&r2, &toCamera, &toCamera);
	Vector_Normalize(&toCamera, &toCamera);
	
	//Fill vertice buffer
	texturePtr = vGouraudBuffer;
	vertexPtr = shape->pointList;
	normalPtr = shape->normalList;
	for(x = 0; x < shape->pointCount; ++x) {
		//Calculate diffuse
		difuse = Vector_DotProduct(&lightSource, normalPtr);
		//KEEP ONLY FOR EDITOR!! --------------------------------------------------------------
		if(difuse < 0) {
			if(shape->backfaceCulling)
			difuse = 0;
			else
			difuse = -difuse;
		}
		difuse = shapeDifuse * (state->ambient + difuse);
		if(difuse > 1.0)
		difuse = 1.0;

		//Calculate specular
		Vector_Multiply(Vector_DotProduct(&lightSource, normalPtr) * 2.0, normalPtr, &v1);
		Vector_Subtract(&v1, &lightSource, &reflectionVector);
		specular = Vector_DotProduct(&reflectionVector, &toCamera);
		if(specular < 0)
		specular = 0;
		
		//Calculate vertex coordinates
		Matrix_TransformVector(&r1, &vertexPtr->point, (VectorPtr) texturePtr);
		
		//Fill vertex
		specular = pow(specular, shapeSpecular);
		texturePtr->r = vertexPtr->u * difuse + specular; if(texturePtr->r > 1.0) texturePtr->r = 1.0;
		texturePtr->g = vertexPtr->v * difuse + specular; if(texturePtr->g > 1.0) texturePtr->g = 1.0;
		texturePtr->b = vertexPtr->c * difuse + specular; if(texturePtr->b > 1.0) texturePtr->b = 1.0;
		texturePtr->a = shape->alpha;
		
		++texturePtr;
		++vertexPtr;
		++normalPtr;
	}
	
	//Fill trianglesBuffer buffer #1
	triPtr = shape->triangleList;
	for(x = 0; x < shape->triangleCount; ++x) {
		//Remove backfacing trianglesBuffer
		if(shape->backfaceCulling) {
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[1]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v1);
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[2]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v2);
			Vector_CrossProduct(&v2, &v1, &n);
			if(Vector_DotProduct((VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &n) > 0) {
				++triPtr;
				continue;
			}
		}

		//Clip triangle Z
		if(clip & kZClipping)
		ClipTriangleGouraud_Z(state, vGouraudBuffer, &verticeCount, trianglesBuffer, &triangleCount, triPtr->corner[0], triPtr->corner[1], triPtr->corner[2]);
		else {
			trianglesBuffer[triangleCount].triangleFlags = 0;
			trianglesBuffer[triangleCount].vertices[0] = triPtr->corner[0];
			trianglesBuffer[triangleCount].vertices[1] = triPtr->corner[1];
			trianglesBuffer[triangleCount].vertices[2] = triPtr->corner[2];
			++triangleCount;
		}
		
		++triPtr;
	}
	
	//Project vGouraudBuffer
	Project_GouraudBuffer(state, verticeCount);
	
	if(clip & kXYClipping) {
		//Clip trianglesBuffer and fill triangle buffer #2
		trianglePtr = trianglesBuffer;
		for(x = 0; x < triangleCount; ++x) {
			ClipTriangleGouraud_XY(state, vGouraudBuffer, &verticeCount, trianglesBuffer2, &triangleCount2, trianglePtr->vertices[0], trianglePtr->vertices[1], trianglePtr->vertices[2]);
			++trianglePtr;
		}
		
		//Draw trianglesBuffer
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount2, trianglesBuffer2);
	}
	else {
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount, trianglesBuffer);
	}
	
	//Flush!
	//QAFlush(state->drawContext);
}

void Gouraud_DrawShape_GlowShading(StatePtr state, ShapePtr shape, MatrixPtr globalPos, MatrixPtr negatedCamera, VectorPtr cameraPos, ShapePtr shapeList[], long clip)
{
	long				x;
	Matrix				r1,
						r2,
						localPos;
	float				difuse,
						shapeDifuse = shape->difuse,
						specular;
	TQAIndexedTriangle	*trianglePtr;
	TQAVGouraud			*texturePtr;
	long				triangleCount,
						triangleCount2,
						verticeCount;
	VertexPtr			vertexPtr;
	VectorPtr			normalPtr;
	TriFacePtr			triPtr;
	Vector				v1,
						v2,
						n,
						lightSource;
	float				l;
	
	//Get local matrix - max depth = 4
	localPos = *globalPos;
	if(shape->parent != nil)
	Shape_LinkMatrixByParent(shape, &localPos);
	if(shape->flags & kFlag_RelativePos)
	Matrix_Cat(&shape->pos, &localPos, &localPos);
	Matrix_Cat(&localPos, negatedCamera, &r1);
	
	//Test visibility
	if(clip == kUndetermined) {
		clip = Box_Visible(&r1, shape->boundingBox, state);
		if(clip == kNotVisible)
		return;
	}
	
	//Calculate specular
	specular = shape->specular * fAbs(TSin(state->frameTime * kPi / (1.0 * kTimeUnit)));
	
	//Create vertice buffer and trianglesBuffer buffer
	verticeCount = shape->pointCount;
	triangleCount = 0;
	triangleCount2 = 0;
	
	//Build lightsource vector
	Matrix_Negate(&localPos, &r2);
	l = FVector_Length(state->lightVector);
	Matrix_RotateVector(&r2, &state->lightVector, &lightSource);
	Vector_Normalize(&lightSource, &lightSource);
	Vector_Multiply(l, &lightSource, &lightSource);
	
	//Fill vertice buffer
	texturePtr = vGouraudBuffer;
	vertexPtr = shape->pointList;
	normalPtr = shape->normalList;
	for(x = 0; x < shape->pointCount; ++x) {
		//Calculate diffuse
		difuse = Vector_DotProduct(&lightSource, normalPtr);
		//KEEP ONLY FOR EDITOR!! --------------------------------------------------------------
		if(difuse < 0) {
			if(shape->backfaceCulling)
			difuse = 0;
			else
			difuse = -difuse;
		}
		difuse = shapeDifuse * (state->ambient + difuse);
		if(difuse > 1.0)
		difuse = 1.0;

		//Calculate vertex coordinates
		Matrix_TransformVector(&r1, &vertexPtr->point, (VectorPtr) texturePtr);
		
		//Fill vertex
		texturePtr->r = vertexPtr->u * difuse + specular; if(texturePtr->r > 1.0) texturePtr->r = 1.0;
		texturePtr->g = vertexPtr->v * difuse + specular; if(texturePtr->g > 1.0) texturePtr->g = 1.0;
		texturePtr->b = vertexPtr->c * difuse + specular; if(texturePtr->b > 1.0) texturePtr->b = 1.0;
		texturePtr->a = shape->alpha;
		
		++texturePtr;
		++vertexPtr;
		++normalPtr;
	}
	
	//Fill trianglesBuffer buffer #1
	triPtr = shape->triangleList;
	for(x = 0; x < shape->triangleCount; ++x) {
		//Remove backfacing trianglesBuffer
		if(shape->backfaceCulling) {
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[1]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v1);
			Vector_Subtract((VectorPtr) &vGouraudBuffer[triPtr->corner[2]], (VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &v2);
			Vector_CrossProduct(&v2, &v1, &n);
			if(Vector_DotProduct((VectorPtr) &vGouraudBuffer[triPtr->corner[0]], &n) > 0) {
				++triPtr;
				continue;
			}
		}

		//Clip triangle Z
		if(clip & kZClipping)
		ClipTriangleGouraud_Z(state, vGouraudBuffer, &verticeCount, trianglesBuffer, &triangleCount, triPtr->corner[0], triPtr->corner[1], triPtr->corner[2]);
		else {
			trianglesBuffer[triangleCount].triangleFlags = 0;
			trianglesBuffer[triangleCount].vertices[0] = triPtr->corner[0];
			trianglesBuffer[triangleCount].vertices[1] = triPtr->corner[1];
			trianglesBuffer[triangleCount].vertices[2] = triPtr->corner[2];
			++triangleCount;
		}
		
		++triPtr;
	}
	
	//Project vGouraudBuffer
	Project_GouraudBuffer(state, verticeCount);
	
	if(clip & kXYClipping) {
		//Clip trianglesBuffer and fill triangle buffer #2
		trianglePtr = trianglesBuffer;
		for(x = 0; x < triangleCount; ++x) {
			ClipTriangleGouraud_XY(state, vGouraudBuffer, &verticeCount, trianglesBuffer2, &triangleCount2, trianglePtr->vertices[0], trianglePtr->vertices[1], trianglePtr->vertices[2]);
			++trianglePtr;
		}
		
		//Draw trianglesBuffer
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount2, trianglesBuffer2);
	}
	else {
		QASubmitVerticesGouraud(state->drawContext, verticeCount, vGouraudBuffer);
		QADrawTriMeshGouraud(state->drawContext, triangleCount, trianglesBuffer);
	}
	
	//Flush!
	//QAFlush(state->drawContext);
}
