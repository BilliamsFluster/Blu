using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


using Blu;
namespace Azure
{
    public class Pawn : Entity
    {
        void OnCreate()
        {
        }

        void OnUpdate(float deltaTime)
        {

            float speed = 1.0f;
            Vec3 velocity = Vec3.Zero;

            if (Input.IsKeyDown(KeyCodes.W))
            {
                velocity.Y = 1.0f;
                Console.WriteLine("W pressed");
            }
            else if (Input.IsKeyDown(KeyCodes.S))
            {
                velocity.Y = -1.0f;
                Console.WriteLine("S pressed");

            }
            if (Input.IsKeyDown(KeyCodes.A))
            {
                velocity.X = -1.0f;
                Console.WriteLine("A pressed");

            }
            else if (Input.IsKeyDown(KeyCodes.D))
            {
                velocity.X = 1.0f;
                Console.WriteLine("D pressed");

            }

            velocity *= speed;
            Vec3 translation = Translation;
            translation += velocity * deltaTime;
            Translation = translation;
        }

    }
}