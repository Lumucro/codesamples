/*
Brent Van Handenhove
PlayerController using RigidBody
Last updated: 22/01/2017
*/

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
[RequireComponent (typeof(Rigidbody))]

public class RigidbodyPlayerController : MonoBehaviour
{
    // Block input, disabling movement
    public bool BlockInput = false;

    // Movement settings
    public Vector3 MaxVelocity = new Vector3(2.5f, 5f, 2.5f);
    public float Speed = 15f;
    public float JumpStrength = 9f;
    public float JumpGroundedDistance = 0.15f;
    public float RotationSpeed = 20f;

    // Fin print settings
    public GameObject FinPrint;
    public float FinPrintDelay = 1f;
    public float FinPrintDespawnTime = 10f;

    //
    // Private variables filled in at runtime, storing key data
    //
    private Vector3 _force;
    private Transform _cameraTransform;
    private Rigidbody _rigidBody;

    private float _distanceToGround;
    private Vector3 _down;
    private RaycastHit _castInfo;
    private bool _isGrounded = false;
    private bool _inWater = false;

    private bool _rightPrint = false;
    private float _lastFinPrint = 0f;
    private Vector3 _finPrintDisplacement = new Vector3(0, 0.05f, 0);
    private float _finSideDisplacement = 0.25f;
        
    // On start
    void Start ()
    {
        // Find camera
        _cameraTransform = GameObject.Find("CameraPivot").transform;
        // Find rigidbody
        _rigidBody = GetComponent<Rigidbody>();
        // Raycasting to ground to find the distance of the center of the collider to the ground
        // We use this for jumping, to check if we're currently grounded
        _distanceToGround = _rigidBody.GetComponent<Collider>().bounds.extents.y / 2f;
        _down = transform.TransformDirection(-Vector3.up);
    }
	
    // Every tick
	void Update ()
    {
        // Find ground
        RaytraceDown();

        // If input is enabled, listen for controls
        if (!BlockInput)
        {
            ListenForJump();
            ListenForMovement();
        }

        // Make sure our velocity does not exceed certain limits
        LimitVelocity();
        // Spawn fin prints if enough time has elapsed
        FinPrints();
    }

    // Listen for input to jump and handle jumping
    void ListenForJump()
    {
        // Jumping
        if (Input.GetButtonDown("Jump") && _isGrounded)
        {
            // Apply upward force
            _rigidBody.AddForce(new Vector3(0, JumpStrength, 0), ForceMode.Impulse);
        }
    }

    // Listen for input to move around and handle it
    void ListenForMovement()
    {
        // Input
        float inputX = Input.GetAxis("Horizontal");
        float inputY = Input.GetAxis("Vertical");

        // Are we on snow? If so, we need to stop any movement (Snow is 100% grip, so stops sliding etc)
        bool onSnow = false;
        if (_castInfo.collider != null)
        {
            onSnow = _castInfo.collider.gameObject.tag == "Snow";
            if (onSnow) _rigidBody.velocity = Vector3.zero;
        }

        // If there is input
        if (inputX != 0 || inputY != 0)
        {
            // Where we go depends on how the camera is facing
            Vector3 originalDir = new Vector3(inputX, 0, inputY);
            // Normalize originalDir

            // Rotation
            float rot = _cameraTransform.eulerAngles.y;
            rot += Vector3.Angle(originalDir, Vector3.forward) * (originalDir.x < 0 ? -1 : 1);

            // Rotate the player to face this direction
            transform.eulerAngles = new Vector3(transform.eulerAngles.x, Mathf.MoveTowardsAngle(transform.eulerAngles.y, rot, RotationSpeed), transform.eulerAngles.z);

            // How we handle movement depends on the surface
            // Snow is 100% grip and needs to feel responsive, so we change the way we handle moving a bit
            // Move forwards
            Vector3 force = transform.TransformDirection(Vector3.forward * Speed * originalDir.magnitude);
            force.y = 0; // Don't affect y velocity

            // As said, snow and ice are handled differently as ice should feel slippery but snow should feel responsive and have grip to it
            // To do this, we make the addforce a bit higher, which makes movement a lot snappier and quicker to react
            if (onSnow)
            {
                _rigidBody.AddForce(force * 5);
            }
            else
            {
                _rigidBody.AddForce(force); }               
            }

    }

    // Limit velocity within configured bounds
    void LimitVelocity()
    {
        // Limit velocity
        Vector3 vel = _rigidBody.velocity;
        if (_rigidBody.velocity.x > MaxVelocity.x) vel.x = MaxVelocity.x;
        if (_rigidBody.velocity.y > MaxVelocity.y) vel.y = MaxVelocity.y;
        if (_rigidBody.velocity.z > MaxVelocity.z) vel.z = MaxVelocity.z;
        if (_rigidBody.velocity.x < -MaxVelocity.x) vel.x = -MaxVelocity.x;
        if (_rigidBody.velocity.y < -MaxVelocity.y) vel.y = -MaxVelocity.y;
        if (_rigidBody.velocity.z < -MaxVelocity.z) vel.z = -MaxVelocity.z;

        // Set velocity to the limited velocity
        _rigidBody.velocity = vel;
    }

    // Check if we're on the floor for jumping, check material for grip / flipper prints etc
    void RaytraceDown()
    {
        // isGrounded if we're on the floor, also pass hitinfo so we can check what kind of surface we're on
        _isGrounded = Physics.Raycast(transform.position, _down, out _castInfo, _distanceToGround + JumpGroundedDistance);

    }

    // Check if we should make a fin print
    void FinPrints()
    {
        // If we're not even on the ground, don't continue checking
        if (!_isGrounded) return;

        // If we're on snow
        if (_castInfo.collider.gameObject.tag == "Snow")
        {
            // Fin prints are on a delay
            _lastFinPrint += Time.deltaTime;

            // If the delay is over, spawn a new fin print
            if (_lastFinPrint >= FinPrintDelay)
            {
                CreateFinPrint();
            }
        }
    }

    // Creates a fin print
    void CreateFinPrint()
    {        
        // Position
        Vector3 pos = _castInfo.point + _finPrintDisplacement;
        if (_rightPrint)
        {
            pos.x += _finSideDisplacement * transform.forward.x;
            pos.z += _finSideDisplacement * transform.forward.z;
        }
        else
        {
            pos.x -= _finSideDisplacement * transform.forward.x;
            pos.z -= _finSideDisplacement * transform.forward.z;
        }

        // Rotation
        Quaternion rot = Quaternion.Euler(90, transform.rotation.eulerAngles.y, 0);

        // Instantiate a new finprint gameobject
        GameObject inst = GameObject.Instantiate(FinPrint, pos, rot) as GameObject;
        // Parent is the object we hit (in case it's moving, so that it also moves with it nicely)
        inst.transform.SetParent(_castInfo.collider.gameObject.transform);
        // Prints despawn after a certain  configured time
        Destroy(inst, FinPrintDespawnTime);

        // Flip if needed (left and right fin)
        if (_rightPrint) inst.transform.localScale = new Vector3(-inst.transform.localScale.x, inst.transform.localScale.y, inst.transform.localScale.z);
        
        // Next fin print is the other side
        _rightPrint = !_rightPrint;
        // Reset timer
        _lastFinPrint -= FinPrintDelay;
    }
     
    // Enter and exit from water
    void OnTriggerEnter(Collider other)
    {
        // If we start colliding with water
        if (other.gameObject.tag == "Water")
        {
            _inWater = true;
            GetComponent<AirLevel>().IsDecreasing = true;
        }
    }

    void OnTriggerExit(Collider other)
    {
        // If we stop colliding with water
        if (other.gameObject.tag == "Water")
        {
            _inWater = false;
            GetComponent<AirLevel>().IsDecreasing = false;
        }
    }
}
