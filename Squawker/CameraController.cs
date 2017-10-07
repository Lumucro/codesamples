/*
Brent Van Handenhove
Camera controller
Last updated: 20/01/2017: Added zooming button, tracking button - Brent
 */

using UnityEngine;
using System.Collections;

public class CameraController : MonoBehaviour
{    
    // Sensitivity config
    [Range(0.1f, 5.0f)]
    public float Sensitivity_Vertical = 0.5f;
    [Range(0.1f, 5.0f)]
    public float Sensitivity_Horizontal = 0.5f;
    // Invert axis config
    [Range(-1, 1)]
    public int Invert_Horizontal = -1;
    [Range(-1, 1)]
    public int Invert_Vertical = 1;
    // Target object to follow
    public GameObject TrackedGameobject;
    // Zooming in and out
    [Range(0, 50)]
    public float ZoomedOutDistance = 10f;
    [Range(0, 10)]
    public float ZoomedInDistance = 1f;

    // Camera distance reference
    public CameraDistance CamDistScriptRef;

    // Base rotation speed
    private float _baseRotationSpeed = 5.0f;
    // Vertical rotation limit
    private float _minVerticalRotation = 5.0f;
    private float _maxVerticalRotation = 85.0f;
    // Zoomed in or not
    private bool _zoomed = false;
    // Track object or not
    private bool _trackObject = true;
    
    // Every tick
    void Update()
    {        
        // Rotate the camera if there is input
        RotateCamera();
        // Zooming if there is input
        ListenForZoom();
        // Tracking the configured object if _trackObject is true
        ListenForTrack();
    }

    // After every tick
    void LateUpdate()
    {
        // We do TrackObject here because our player is a RigidBody
        // If we do this in Update, we sometimes get a jitter in the camera movement
        // To prevent this, run the code after Tick
        TrackObject();
    }

    // Rotate camera function
    void RotateCamera()
    {
        // Get input
        float hor = Input.GetAxis("HorizontalR");
        float vert = Input.GetAxis("VerticalR");
        float mouseHor = Input.GetAxis("Mouse X");
        float mouseVert = Input.GetAxis("Mouse Y");

        // Horizontal rotation       
        transform.Rotate(0, (hor + mouseHor)  * Sensitivity_Horizontal * _baseRotationSpeed * Invert_Horizontal, 0, Space.World);
        // Vertical rotation
        float verticalRotation = vert + mouseVert * Sensitivity_Vertical * _baseRotationSpeed * Invert_Vertical;
        float newRotation = transform.rotation.eulerAngles.x + verticalRotation;
        // Clamp vertical rotation
        if (newRotation < _minVerticalRotation) verticalRotation = _minVerticalRotation - transform.rotation.eulerAngles.x;
        if (newRotation > _maxVerticalRotation) verticalRotation = _maxVerticalRotation - transform.rotation.eulerAngles.x;
        // Apply vertical rotation to camera
        transform.Rotate(verticalRotation, 0, 0);
    }

    // Track object
    void TrackObject()
    {
        // If there is an object to track
        if (TrackedGameobject != null && _trackObject)
        {
            // Smooth movement towards target
            Vector3 targetPos = Vector3.MoveTowards(transform.position, TrackedGameobject.transform.position, 0.1f);
            transform.position = targetPos;
        }
    }

    // Zoom in and out
    void ListenForZoom()
    {
        // Zooming button
        if (Input.GetButtonUp("Zoom"))
        {
            // If zoom button is released, toggle zoom enabled or not
            _zoomed = !_zoomed;
            if (_zoomed)
            {
                // Tell the camera distance script that we are now zoomed out
                CamDistScriptRef.CurrentDistance = ZoomedOutDistance;
            }
            else
            {
                // Tell the camera distance script that we are now zoomed out
                CamDistScriptRef.CurrentDistance = ZoomedInDistance;
            }
        }
    }

    // Track object or center
    void ListenForTrack()
    {
        // Tracking button
        if (Input.GetButtonDown("LockToCenter"))
        {
            // Toggle tracking of object
            _trackObject = !_trackObject;

            // If we don't track the object anymore
            if (!_trackObject)
            {
                // Set the camera to the center of the level (which is always 0, 0, 0)
                transform.position = Vector3.zero;
                // Zoom out, too
                CamDistScriptRef.CurrentDistance = ZoomedOutDistance;
            }
        }
    }
}
