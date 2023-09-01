using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Blu;
namespace Azure
{
    public class Actor : Entity
    {
        protected Actor() {  }
        public Actor(ulong id) : base(id)
        {
        }
        void OnCreate()
        {
            Console.WriteLine("Actor Create");

        }

        void OnUpdate(float deltaTime)
        {
        }

        
    }
}
