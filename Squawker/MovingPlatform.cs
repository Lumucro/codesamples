/*
Brent Van Handenhove
Moving Platform logic
Last updated: 20/01/2017: Bugfix, added parenting support - Brent
*/

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[RequireComponent(typeof(Rigidbody))]
public class MovingPlatform : MonoBehaviour
{
    // Public settings
    public bool Enabled = true;
    public List<GameObject> Waypoints;
    public float WaitTime = 2.f;
    public float Speed = 5.f;
    public bool IgnoreY = true;

    //
    // Private variables to store key data at runtime
    //

    private int _currentWaypoint = 0;
    private int _amountOfWaypoints;
    private Rigidbody _rigidBody;
    private float _waitedTime;
   
    void Start()
    {
        // Get waypoints count
        _amountOfWaypoints = Waypoints.Count;
        // Get rigidbody
        _rigidBody = GetComponent<Rigidbody>();
    }

    // Fixed update because physics are involved
    void FixedUpdate()
    {
        // If the platform is enabled
        if (Enabled)
        {            
            // Get target waypoint position, preserve Y position (it is not important for our use)
            Vector3 waypointpos = Waypoints[_currentWaypoint].transform.position;
            Vector3 target = new Vector3(waypointpos.x, transform.position.y, waypointpos.z);

            // If we are at the waypoint
            if (transform.position == target)
            {
                // Wait a bit, this is so that the player has a few seconds to get on
                _waitedTime += Time.deltaTime;

                // If we've waited longe nough
                if (_waitedTime >= WaitTime)
                {
                    // Move to the next waypoint
                    _currentWaypoint = (++_currentWaypoint) % _amountOfWaypoints;
                    _waitedTime = 0;
                }
            }
            
            // Lerp to the target position
            Vector3 pos = Vector3.MoveTowards(transform.position, target, Time.deltaTime * Speed);
            _rigidBody.MovePosition(pos);
        }
    }

    // If we touch the platform
    void OnCollisionEnter(Collision other)
    {
        // Make the other object a child of this one so that it moves along
        other.gameObject.transform.parent = this.gameObject.transform;
    }

    // If we get off the platform
    void OnCollisionExit(Collision other)
    {
        // Clear the other objects parent
        other.gameObject.transform.parent = null;
    }
}
