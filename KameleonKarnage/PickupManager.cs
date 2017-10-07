//Pickupmanager
//Handles pickup spawning and respawning
//Last edited 07/03/2017 by Brent

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PickupManager : MonoBehaviour {

    //
    //Public vars
    //
    public List<Transform> SpawnPositions; //List of locations where a pickup can spawn
    public int MaxPickupsSpawned = 5; //Max pickups to spawn
    public float RespawnTime = 8f; //Delay before a new pickup spawns
    public GameObject PickupPrefab; //Pickup prefab

    //
    //Private vars
    //
    private List<GameObject> _pickups; //List of spawned pickups
    private float _respawnTimer = 0f; //Elapsed time for respawn check

    //Respawn pickups
    public void RespawnPickups()
    {
        ClearPickups(); //Clear existing pickups

        // Spawn pickups until we have reached the maximum
        for (int i = 0; i < MaxPickupsSpawned; ++i)
        {
            SpawnPickup();
        }
    }

    //Spawn pickups on start
    void Start()
    {
        RespawnPickups();
    }

    //Spawn pickups on delay
    void Update()
    {
        _respawnTimer += Time.deltaTime;
    }
    
    //Clear existing pickups
    void ClearPickups()
    {
        //Destroy all gameobjects
        foreach (GameObject obj in _pickups)
        {
            Destroy(obj);
        }

        //Clear the list
        _pickups.Clear();
    }

    //Spawn a pickup
    void SpawnPickup()
    {
        //Check if we're not at our max amount of allowed pickups spawned
        if (GameObject.FindGameObjectsWithTag("Pickup").Length < MaxPickupsSpawned)
        {
            //Loop through the spawnpoints
            bool keepGoing = true;
            //Check if there are any pickups already spawned here
            //If there are, continue until we find an empty spawn (if there are none, don't spawn pickup)
            foreach (Transform t in SpawnPositions)
            {
                //Check if any of the pickups spawned already spawned at this point
                foreach (GameObject obj in _pickups)
                {
                    if ((obj.transform.position - t.position).magnitude < Mathf.Epsilon)
                    {
                        keepGoing = false;
                        break; //Escape foreach
                    }
                }
                //No objects were here, spawn one
                if (keepGoing)
                {
                    GameObject obj = Instantiate(PickupPrefab, t.position, Quaternion.identity); //Spawn obj
                    obj.GetComponent<ColorPickup>().PickupColor = RandomColor(); //Set color here;
                    _pickups.Add(obj); //Add obj to list of pickups
                }
            }
        }
    }
}
