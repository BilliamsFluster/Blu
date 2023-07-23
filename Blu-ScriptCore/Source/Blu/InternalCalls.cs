﻿using System;
using System.Runtime.CompilerServices;


namespace Blu
{
    public static class InternalCalls
    {

        #region TransformComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);
        #endregion

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsKeyDown(KeyCodes keycode);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);


        #region Rigidbody2DComponent
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, bool wake);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 linearVelocity);
        //[MethodImplAttribute(MethodImplOptions.InternalCall)]
        //internal extern static Rigidbody2DComponent.BodyType Rigidbody2DComponent_GetType(ulong entityID);
        //[MethodImplAttribute(MethodImplOptions.InternalCall)]
        //internal extern static void Rigidbody2DComponent_SetType(ulong entityID, Rigidbody2DComponent.BodyType type);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, ref Vector2 impulse, bool wake);
        #endregion

    }
}