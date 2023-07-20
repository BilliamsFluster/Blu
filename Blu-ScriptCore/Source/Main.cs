using System;
using System.Runtime.CompilerServices;

namespace Blu
{
    public struct Vec3
    {
        public float X, Y, Z;

        public Vec3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }
    public static class InternalCalls
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Log(string text, int parameter);

    }
    public class Entity
    {
        public Entity()
        {
           
        }

        public void PrintMessage()
        {
            Console.WriteLine("Hello World from C#");
        }
       

        
    }

}