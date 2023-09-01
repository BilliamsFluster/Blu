using Azure;
using System;
using System.Runtime.CompilerServices;


namespace Blu
{
    

    
    public class Entity
    {
        protected Entity() { ID = 0; }
        public Entity(ulong id)
        {
            ID = id;
        }

        public readonly ulong ID;
        public Vector3 Translation
        {
            get
            {
                InternalCalls.TransformComponent_GetTranslation(ID, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.TransformComponent_SetTranslation(ID, ref value);
            }
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            Type componentType = typeof(T);
            return InternalCalls.Entity_HasComponent(ID, componentType);
        }


        public T GetComponent<T>() where T : Component, new()
        {
            if( !HasComponent<T>())
                return null;
            T component = new T() { Entity = this };
            return component;
            
            
        }

        public Entity FindEntityByName(string name)
        {
            ulong id = InternalCalls.Entity_FindEntityByName(name);
            Console.WriteLine(id);
            if (id == 0)
                return null;
            return new Entity(id);
        }

        public T As<T>() where T : Entity, new() 
        {
            object instance = InternalCalls.GetScriptInstance(ID);
            return instance as T;
        }



    }

}