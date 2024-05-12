using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Blu;
namespace Azure
{
    public class Camera : Actor
    {
        public Entity OtherEntity;
        public float DistanceFromPlayer = 5.0f;

        void OnCreate()
        {
            Translation = new Vector3(Translation.XY, DistanceFromPlayer);
        }
        void OnUpdate(float deltaTime)
        {
            Entity player = FindEntityByName("BoxPawn");
            if(player != null)
            {
                
                Translation = new Vector3 (player.Translation.XY, DistanceFromPlayer);

            }
            else
            {

            }
        }
    }
}
