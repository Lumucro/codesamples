//RoundController
//Handles round mechanics
//Last edited 18/06/2017 by Brent

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;



//Enumeration for roundstates
public enum RoundState
{
    PRE, ACTIVE, POST, GAMEOVER
}

[RequireComponent(typeof(PlayerSpawner))]

public class RoundController : MonoBehaviour {

    // Delegate so other scripts can add Resetfunctions to the roundcontroller
    public delegate void ResetAtRound();

    //
    //Public vars
    //
    public ResetAtRound ResetPreRound; //function to call at preRound (other scripts)
    public ResetAtRound ResetPostRound; //function to call at PostRound (other scripts)
    public ResetAtRound ResetActiveRound; //function to call at ActiveRound (other scripts)
 
    public int RoundsToWin = 3; //How many rounds does a player need to win to win the game
    public float FirstPreRoundTime = 15f; //Time before FIRST round starts
    public float PreRoundTime = 5f; //Time before round so that everyone is ready
    public float RoundTime = 10f; //Max amount of seconds a round can go on for
    public float PostRoundTime = 5f; //Time after rounds to wait (let the winner gloat!)
    public RoundState CurrentRoundState = RoundState.PRE; //Pre-round state for starters, current roundstate
    //UI
    public GameObject PreRoundPanel; //Before round panel
    public Text PreRoundTimerText; //Text of preround time
    public GameObject RoundPanel; //Round panel
    public Text RoundTimerText; //Text of round time
    public GameObject PostRoundPanel; //Post round panel
    public Text PostRoundPlayerText; //Text of winning player
    public Text DrawText; //Text of a draw
    //WINSCREEN
    public GameObject WinScreenCameraObject;//the camera of the winscreen in the level
    public GameObject PlayerWinPrefab; // the model of the WinPlayer
    public GameObject[] PlayerPedestals; //The pedestals for the players
    public float WaitForPedestalsTime = 2.0f;//the time it takes for the pedestals to start rising
    public float PedestalsTravelTime = 4.0f; //the time it takes for the pedestals to go up
    public float GoBackToMenuTime = 5.0f; //the time it takes to go back to menu after winning
    public float MaxpedestalHeigth = 10.0f;//the maximum amount the pedestals will go up;

    //
    //Private vars
    //
    private Camera _mainLevelCamera;// the camera of the arena
    private int _currentRound = 0; //The how manieth round is this
    private PlayerSpawner _playerSpawner; //Reference to PlayerSpawner
    private float _roundTime = 0f; //Time for this roundphase
    private float _roundTimeElapsed = -10f; //Elapsed time in this round so far
    private int _livePlayers = 0; //How many players are still alive
    private int[] _playerScores = new int[4] { 0, 0, 0, 0 }; //Rounds won by each player
    private bool _gameOver = false;
    private GameObject[] _playerWinModels; //The models of the players on the winscreen;
    private float _winScreenTime = 0.0f;

    //Check if the round is over (because, for example, all players but 1 are dead)
    public void CheckRoundEnded()
    {
        //Check if enough players are alive to continue the round
        if (_livePlayers <= 1)
        {
            //Is there a winner or is it a draw? (rare, but plausible)
            if (_livePlayers == 1)
            {
                //Find the player that's still alive
                GameObject player = GameObject.FindGameObjectWithTag("Player");
                ++_playerScores[player.GetComponent<PlayerController>().PlayerID];
                PostRoundPlayerText.enabled = true;
                DrawText.enabled = false;
            }
            else
            {
                DrawText.enabled = true;
                PostRoundPlayerText.enabled = false;
            }

            //End round
            SetRoundState(RoundState.POST);
        }
    }

    //A player died
    public void PlayerDied()
    {
        if (CurrentRoundState != RoundState.ACTIVE) return;
        --_livePlayers;
        CheckRoundEnded();
    }

	//Get our playerspawner
	void Start ()
	{
	    _winScreenTime = GoBackToMenuTime + PedestalsTravelTime + WaitForPedestalsTime;
	    _mainLevelCamera = Camera.main;
        _playerSpawner = gameObject.GetComponent<PlayerSpawner>();
        _roundTime = FirstPreRoundTime; //After we just load in, we wait a tiny bit longer before we start the round;

        //spawns the player win models
        _playerWinModels = new GameObject[PlayerManager.AmountOfPlayers];
	    for (int i = 0; i < _playerWinModels.Length; ++i)
	    {
	        _playerWinModels[i] = Instantiate(PlayerWinPrefab, PlayerPedestals[i].transform.position, Quaternion.identity);
            _playerWinModels[i].GetComponentInChildren<Renderer>().materials[1].SetColor("_EmissionColor", PlayerManager.PlayerColors[i] *5.0f);
	        _playerWinModels[i].GetComponentInChildren<Text>().text = PlayerManager.PlayerNames[i];
            _playerWinModels[i].transform.parent = PlayerPedestals[i].transform;
	    }

        //Reset just in case
        _currentRound = 0;
        _roundTimeElapsed = 0f;

        StartPreRound(); //Start the round
    }

    //Timer for round time / pre time / post time
    void Update()
    {
        //Add deltatime to elapsed roundtime
        _roundTimeElapsed += Time.deltaTime;

        //Update UI element
        switch (CurrentRoundState)
        {
            case RoundState.PRE:
                if (PreRoundTimerText)
                    PreRoundTimerText.text = (Mathf.CeilToInt(_roundTime - _roundTimeElapsed)).ToString();
                break;
            case RoundState.ACTIVE:
                if (RoundTimerText)
                    RoundTimerText.text = (Mathf.CeilToInt(_roundTime - _roundTimeElapsed)).ToString();
                break;
        }

        CheckRoundTimer(); //Check if round is over
    }

    //Start a preround by spawning the players
    void StartPreRound()
    {
        //Hide the other UI panels
        if (RoundPanel)
            RoundPanel.SetActive(false);
        if (PostRoundPanel)
            PostRoundPanel.SetActive(false);
        if (PreRoundPanel)
            PreRoundPanel.SetActive(true);

        //Kill all existing players
        foreach (GameObject obj in GameObject.FindGameObjectsWithTag("Player"))
        {
            DestroyImmediate(obj);
        }

        _playerSpawner.SpawnPlayers(); //Spawn players

        //Disable input
        foreach (GameObject obj in GameObject.FindGameObjectsWithTag("Player"))
        {
            obj.GetComponent<PlayerController>().LockedInput = true;
            obj.GetComponent<ShootController>().LockedInput = true;
        }

        //Amount of live players = amount of total players
        _livePlayers = PlayerManager.AmountOfPlayers;

        //Call the resetfunctions of other scripts
        if(ResetPreRound != null)ResetPreRound.Invoke();
    }

    //Start the round by unfreezing the players
    void StartRound()
    {
        //Hide the other UI panels
        if (RoundPanel)
            RoundPanel.SetActive(true);
        if (PostRoundPanel)
            PostRoundPanel.SetActive(false);
        if (PreRoundPanel)
            PreRoundPanel.SetActive(false);

        //Enable input
        foreach (GameObject obj in GameObject.FindGameObjectsWithTag("Player"))
        {
            obj.GetComponent<PlayerController>().LockedInput = false;
            obj.GetComponent<ShootController>().LockedInput = false;
        }

        //Call the resetfunctions of other scripts
        if (ResetActiveRound != null) ResetActiveRound.Invoke();
    }

    //End a round
    void EndRound()
    {
        //Call the resetfunctions of other scripts
        if (ResetPostRound != null) ResetPostRound.Invoke();

        //Hide the other UI panels
        if (RoundPanel)
            RoundPanel.SetActive(false);
        if (PreRoundPanel)
            PreRoundPanel.SetActive(false);
        if (PostRoundPanel)
            PostRoundPanel.SetActive(true);

        //Set the current winning player
        GameObject[] objarr = GameObject.FindGameObjectsWithTag("Player");
        int winner = 0;
        float highestHealth = float.MinValue;

        foreach (GameObject objec in objarr)
        {
            if (objec)
            {
                float currentHealth = objec.GetComponent<HealthController>().CurrentHealth;
                if (currentHealth > highestHealth)
                {
                    highestHealth = currentHealth;
                    winner = (int)objec.GetComponent<PlayerController>().PlayerID;
                }
            }
        }

        ++_playerScores[winner];
        PostRoundPlayerText.text = PlayerManager.PlayerNames[winner];
        //checks if we should go to another round
        if (_playerScores[winner] >= RoundsToWin)
        {
            _gameOver = true;

        }

        ++_currentRound; //Next round
    }

    //Check the roundtimer to go to the next state of the round or not
    void CheckRoundTimer()
    {
        if (_roundTimeElapsed >= _roundTime)
        {
            switch (CurrentRoundState)
            {
                case RoundState.PRE:
                    SetRoundState(RoundState.ACTIVE);
                    break;
                case RoundState.ACTIVE:
                    SetRoundState(RoundState.POST);
                    break;
                case RoundState.POST:
                    if (_gameOver)
                    {
                        SetRoundState(RoundState.GAMEOVER);
                        break;
                    }
                    SetRoundState(RoundState.PRE);
                    break;
                case RoundState.GAMEOVER:
                    Debug.Log("Load Lobby Scene");
                    SceneManager.LoadScene("Lobby");//loads the main menu
                    break;
            }
        }
    }

    //Change the roundstate
    void SetRoundState(RoundState newState)
    {
        //Roundstate var
        CurrentRoundState = newState;

        //Set the RoundTime to the appropriate one
        switch (newState)
        {
            case RoundState.PRE:
                StartPreRound();
                _roundTime = PreRoundTime;
                break;
            case RoundState.ACTIVE:
                StartRound();
                _roundTime = RoundTime;
                break;
            case RoundState.POST:
                EndRound();
                _roundTime = PostRoundTime;
                break;
            case RoundState.GAMEOVER:
                //Change the camera to the winscreen camera
                WinScreenCameraObject.SetActive(true);
                _mainLevelCamera.gameObject.SetActive(false);
                break;
        }

        //Reset timer
         _roundTimeElapsed = 0;
    }
}
