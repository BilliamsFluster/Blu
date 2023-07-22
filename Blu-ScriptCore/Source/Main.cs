using System;
using System.Runtime.CompilerServices;


namespace Blu
{
    public struct Vec3
    {
        public float X, Y, Z;

        public static Vec3 Zero = new Vec3(0.0f, 0.0f, 0.0f);

        public Vec3(float scalar)
        {
            X = scalar;
            Y = scalar;
            Z = scalar;
        }
        public Vec3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
        static public Vec3 operator+(Vec3 vectorA, Vec3 vectorB)
        {
            return new Vec3(vectorA.X + vectorB.Z, vectorA.Y + vectorB.Z, vectorA.Z + vectorB.Z);
        }
        static public Vec3 operator *(Vec3 vector, float scalar)
        {
            return new Vec3(vector.X * scalar, vector.Y * scalar, vector.Z * scalar);
        }
    }

    public static class InternalCalls
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Log(string text, int parameter);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Entity_GetTranslation(ulong entityID, out Vec3 translation);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Entity_SetTranslation(ulong entityID, ref Vec3 translation);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsKeyDown(KeyCodes keycode);

    }
    public class Entity
    {
        protected Entity() { ID = 0; }
        public Entity(ulong id)
        {
            ID = id;
        }

        public readonly ulong ID;
        public Vec3 Translation
        {
            get
            {
                InternalCalls.Entity_GetTranslation(ID, out Vec3 translation);
                return translation;
            }
            set
            {
                InternalCalls.Entity_SetTranslation(ID, ref value);
            }
        }


        
       

        
    }

}