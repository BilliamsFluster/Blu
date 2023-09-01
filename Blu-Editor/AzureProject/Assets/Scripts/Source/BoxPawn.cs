using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


using Blu;
namespace Azure
{
    public class BoxPawn : Pawn
    {
        private TransformComponent m_Transform;
        private Rigidbody2DComponent m_Rigidbody;
        public float Speed = 0;

        private Entity cameraEntity;
        private Camera camera;
        void OnCreate()
        {
            m_Transform = GetComponent<TransformComponent>();
            m_Rigidbody = GetComponent<Rigidbody2DComponent>();
            


        }

        void OnUpdate(float deltaTime)
        {
            

            cameraEntity = FindEntityByName("Camera");
            if (cameraEntity != null)
            {
                camera = cameraEntity.As<Camera>();

            }
            
            Vector3 velocity = Vector3.Zero;

            if (Input.IsKeyDown(KeyCodes.W))
            {
                velocity.Y = 0.1f;
                Console.WriteLine("W pressed");
            }
            else if (Input.IsKeyDown(KeyCodes.S))
            {
                velocity.Y = -0.1f;
                Console.WriteLine("S pressed");

            }
            if (Input.IsKeyDown(KeyCodes.A))
            {
                velocity.X = -0.1f;
                 
                Console.WriteLine("A pressed");

            }
            else if (Input.IsKeyDown(KeyCodes.D))
            {
                velocity.X = 0.1f;
                Console.WriteLine("D pressed");

            }
            if (Input.IsKeyDown(KeyCodes.Q))
            {
                camera.DistanceFromPlayer += Speed * deltaTime;
            }
            else if (Input.IsKeyDown(KeyCodes.E))
            {
                camera.DistanceFromPlayer -= Speed * deltaTime;

            }
            velocity *= Speed;
            m_Rigidbody.ApplyLinearImpulse(velocity.XY, Vector2.Zero, true);

        }

    }
}
