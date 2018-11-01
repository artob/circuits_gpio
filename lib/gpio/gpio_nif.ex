defmodule Circuits.GPIO.Nif do
  @on_load {:load_nif, 0}
  @compile {:autoload, false}

  @moduledoc false

  def load_nif() do
    priv_dir = Application.app_dir(:circuits_gpio, "priv")
    nif_binary = Path.join(priv_dir, "gpio_nif")

    case :erlang.load_nif(to_charlist(nif_binary), 0) do
      :ok ->
        helper_binary = Path.join(priv_dir, "gpio_helper")
        if File.exists?(helper_binary), do: set_helper(helper_binary)
        :ok

      error ->
        error
    end
  end

  def set_helper(_helper) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def open(_pin_number, _pin_direction) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def read(_gpio) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def write(_gpio, _value) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def set_edge_mode(_gpio, _edge, _suppress_glitches, _process) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def set_direction(_gpio, _pin_direction) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def set_pull_mode(_gpio, _pull_mode) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def pin(_gpio) do
    :erlang.nif_error(:nif_not_loaded)
  end

  def info() do
    :erlang.nif_error(:nif_not_loaded)
  end
end
