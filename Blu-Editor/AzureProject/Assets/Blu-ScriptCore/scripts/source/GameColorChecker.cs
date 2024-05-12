using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


using Blu;
namespace Azure
{
    public class GameRigidBodyChecker: Entity
    {
        private TransformComponent m_Transform;
        private Rigidbody2DComponent m_Rigidbody;
        
        

        private Entity cameraEntity;
        private Camera camera;

        
        void OnCreate()
        {
            m_Transform = GetComponent<TransformComponent>();
            m_Rigidbody = GetComponent<Rigidbody2DComponent>();



        }

        void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyDown(KeyCodes.I))
            {

            }
            
        }

        public static void PrintAllEntityIDsToConsole()
        {
            List<Entity> ids = GetAllEntities();

            foreach (var id in ids)
            {
                string name = id.GetComponent<TagComponent>().GetName();
                Console.WriteLine($"Entity ID and component: {id}, {name}");
            }
        }



    }
}

