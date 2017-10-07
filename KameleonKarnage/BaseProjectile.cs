//BaseProjectile
//Handles projectiles
//Last edited 25/04/2017 by Brent

using System.Collections;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Security.Policy;
using UnityEngine;
using GamepadInput;

public class BaseProjectile : MonoBehaviour {

    //
    //Public vars
    //
    public float Damage = 1; //Projectile damage
    public float Speed = 200f; //Projectile speed
    public Vector3 Direction = Vector3.zero; //Projectile direction
    public float BulletCost; //a way to get the bulletcost;
    public GameObject OriginPlayer; //a way to get the original player who shot the bullet
    public int MaxAmountOfBounces = 1;// the maximum amount of bounce the bullet has
    public float Radius = 1.0f; // the radius of the bullet
    public float ChargeSize = 0.5f; //the max increase in size the bullet makes when chargedw
    //
    //Private vars
    //
    protected Color _color = Color.white;
    protected Vector3 _prevPosition; 
    protected int _reflects = 0;//Amount of reflects                            
    protected AutoDestroy _autoDestroy; //Ref to autodestroy
    protected Vector3 _raycastOffset = new Vector3(0, 2.0f, 0); //an offset so the bullets don't collide with the ground

    // On start
    public virtual void Start()
    {
        // Get references that we need
        Direction = transform.forward;
        _prevPosition = transform.position;
        _autoDestroy = GetComponent<AutoDestroy>();
    }

    //Change the projectile color
    public virtual void SetColor(Color col)
    {
        _color = col; //Update stored color
        var renderers = GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            renderer.material.SetColor("_EmissionColor", _color); //Set emission color of material to col
        }

        var lights = GetComponentsInChildren<Light>();
        foreach (var light in lights)
        {
            light.color = _color;
        }
    }

    //Get the projectile color
    public virtual Color GetColor()
    {
        return _color;
    }
	
	//Move the bullet forward
	public virtual void Update () 
    {
        transform.Translate(Direction* Time.deltaTime * Speed, Space.World);
        HitDetection();
    }

    //When we hit something, we need to damage it (if its a player) or destroy ourselves (if its a solid)
    //needs to become a raycast to the previous position in update;
    protected virtual void HitDetection()
    {
        // Raycast to see if the projectile hit something
        RaycastHit hit;
        float distance = (transform.position - _prevPosition).magnitude;
        if (_prevPosition == transform.position) _prevPosition -= Direction;

        //Check a hit between prev pos and current pos if so check what we have to do        
        if (Physics.SphereCast(new Ray(_prevPosition + _raycastOffset, Direction), Radius, out hit, distance))
        {
            // Get the hit object
            GameObject gameObject = hit.collider.gameObject;

            //If we hit a player
            if (hit.collider.CompareTag("Player"))
            {
                if (gameObject != OriginPlayer) 
                {
                    //Damage the hit player
                    var controller = gameObject.GetComponent<HealthController>();
                    if (controller) controller.Damage(Damage, _color);

                    //Destroy the projectile
                    Destroy(gameObject);
                }
            }
            else
            {
                //We hit something else (probably terrain or an obstacle), check to see if we should destroy or reflect
                CheckBounce(hit);
            }
        }

        _prevPosition = transform.position;
    }

    //Checks if the projectile can still reflect or not, depending on max reflects and amounts already reflected
    protected void CheckBounce(RaycastHit hit)
    {
        if (_reflects < MaxAmountOfBounces)
        {
           Reflect(hit);
        }
        else
        { 
            FancyDestroy();
        }
    }

    //Code to reflect our shot / bounce
    private void Reflect(RaycastHit hit)
    {
        //Reflect the dir vector
        Direction = Vector3.Reflect(transform.forward, hit.normal);

        //Set the position of the projectile to the collision point, to prevent any jittering
        Vector3 position = hit.point;
        position.y = transform.position.y;
        transform.position = position;
        
        //Update amount of times reflected
         ++_reflects;

        // Reset autodestroy, reflecting counts as a new 'shot'
        if (_autoDestroy != null) _autoDestroy.ResetDestroyTimer();
    }
}
