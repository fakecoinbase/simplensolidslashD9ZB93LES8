// OET token
pragma solidity 0.6.3;

abstract contract Token
{
	// Optional
	// function name() public view virtual returns (string _name);
	// function symbol() public view virtual returns (string _symbol);
	// function decimals() public view virtual returns (uint8 _decimals);
	function totalSupply() public view virtual returns (uint256 _totalSupply);
	function balanceOf(address _owner) public view virtual returns (uint256 _balance);
	function transfer(address _to, uint256 _value) public virtual returns (bool _success);
	function transferFrom(address _from, address _to, uint256 _value) public virtual returns (bool _success);
	function approve(address _spender, uint256 _value) public virtual returns (bool _success);
	function allowance(address _owner, address _spender) public view virtual returns (uint256 _remaining);
	event Transfer(address indexed _from, address indexed _to, uint256 _value);
	event Approval(address indexed _owner, address indexed _spender, uint256 _value);
}

contract StandardToken is Token
{
	uint256 supply;
	mapping (address => uint256) balances;
	mapping (address => mapping (address => uint256)) allowed;

	function totalSupply() public view override returns (uint256 _totalSupply)
	{
		return supply;
	}

	function balanceOf(address _owner) public view override returns (uint256 _balance)
	{
		return balances[_owner];
	}

	function transfer(address _to, uint256 _value) public override returns (bool _success)
	{
		require(msg.data.length >= 68); // fix for short address attack
		address _from = msg.sender;
		require(_value > 0);
		require(balances[_from] >= _value);
		assert(balances[_to] + _value > balances[_to]);
		balances[_from] -= _value;
		balances[_to] += _value;
		emit Transfer(_from, _to, _value);
		return true;
	}

	function transferFrom(address _from, address _to, uint256 _value) public override returns (bool _success)
	{
		address _spender = msg.sender;
		require(_value > 0);
		require(balances[_from] >= _value);
		require(allowed[_from][_spender] >= _value);
		assert(balances[_to] + _value > balances[_to]);
		balances[_from] -= _value;
		allowed[_from][_spender] -= _value;
		balances[_to] += _value;
		emit Transfer(_from, _to, _value);
		return true;
	}

	function approve(address _spender, uint256 _value) public override returns (bool _success)
	{
		address _owner = msg.sender;
		require(_value >= 0);
		allowed[_owner][_spender] = _value;
		emit Approval(_owner, _spender, _value);
		return true;
	}

	function allowance(address _owner, address _spender) public view override returns (uint256 _remaining)
	{
		return allowed[_owner][_spender];
	}
}

contract OET is StandardToken
{
	function name() public pure returns (string memory _name)
	{
		return "Over Eosio Token";
	}

	function symbol() public pure returns (string memory _symbol)
	{
		return "OET";
	}

	function decimals() public pure returns (uint8 _decimals)
	{
		return 2;
	}
}
