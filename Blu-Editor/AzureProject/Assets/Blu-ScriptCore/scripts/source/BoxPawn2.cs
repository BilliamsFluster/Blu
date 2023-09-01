using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


using Blu;
namespace Azure
{
    public class BoxPawn2 : Pawn
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

            if (Input.IsKeyDown(KeyCodes.Up))
            {
                velocity.Y = 0.1f;
            }
            else if (Input.IsKeyDown(KeyCodes.Down))
            {
                velocity.Y = -0.1f;

            }
            if (Input.IsKeyDown(KeyCodes.Left))
            {
                velocity.X = -0.1f;
            }
            else if (Input.IsKeyDown(KeyCodes.Right))
            {
                velocity.X = 0.1f;

            }
            
            velocity *= Speed;
            m_Rigidbody.ApplyLinearImpulse(velocity.XY, Vector2.Zero, true);

        }

    }
}
